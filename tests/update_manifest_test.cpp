#include "UpdateManifest.h"
#include <cassert>
#include <iostream>
#include <random>

using namespace filespacer;

int main() {
    const std::string prefix = "FSUP\n01000500\n"
        "https://github.com/LeBenco/FileSpacer/releases/tag/v1.0.5";
    const std::string installer = "https://github.com/LeBenco/FileSpacer/releases/download/"
        "v1.0.5/FileSpacer-1.0.5-install.exe";
    const std::string hash(64, 'a');
    const std::string full = prefix + "\n" + installer + "\n" + hash + "\n12345\n";
    ReleaseManifest parsed;
    assert(parseUpdateManifest(prefix, &parsed));
    assert(parsed.version == 0x01000500 && parsed.installerUrl.empty());
    assert(parseUpdateManifest(prefix + "\n", &parsed));
    assert(parseUpdateManifest(full, &parsed));
    assert(parsed.installerUrl == installer && parsed.sha256 == hash && parsed.installerSize == 12345);
    assert(parseUpdateManifest(prefix + "\n" + installer + "\n" + std::string(64, 'F')
        + "\n268435456\n", &parsed));
    assert(parsed.sha256 == std::string(64, 'f') && parsed.installerSize == MAX_INSTALLER_SIZE);
    // Legacy checks reset installer fields instead of retaining a previous executable.
    assert(parseUpdateManifest(prefix, &parsed) && parsed.installerSize == 0 && parsed.sha256.empty());
    auto reject = [&](std::string text) {
        ReleaseManifest unchanged;
        unchanged.url = "sentinel";
        assert(!parseUpdateManifest(text, &unchanged));
        assert(unchanged.url == "sentinel" && unchanged.version == 0);
    };
    reject(""); reject("FSUP\n"); reject(full + "extra\n"); reject(full + "\n");
    reject(prefix + "\n" + installer); reject(full.substr(0, full.find(hash) + 63));
    auto replaced = [&](const std::string &from, const std::string &to) {
        std::string text = full; const auto pos = text.find(from); assert(pos != std::string::npos);
        text.replace(pos, from.size(), to); reject(text);
    };
    replaced("FSUP", "BAD!"); replaced("01000500", "0100050g");
    replaced("/tag/v1.0.5", "/tag/v1.0.6"); replaced("https:", "http:");
    replaced("github.com/LeBenco", "github.com.evil/LeBenco");
    replaced("/download/v1.0.5", "/download/v1.0.6");
    replaced("FileSpacer-1.0.5-install.exe", "FileSpacer-1.0.6-install.exe");
    replaced(hash, std::string(64, 'g')); replaced(hash, std::string(63, 'a'));
    replaced("12345", "0"); replaced("12345", "-1"); replaced("12345", "12a45");
    replaced("12345", "268435457"); replaced("12345", "18446744073709551615");
    replaced("12345", "12345 "); replaced("\n", "\r\n");
    replaced("/FileSpacer-", "/../FileSpacer-");
    replaced("/tag/v1.0.5", "/tag/v1.0.5?x=1");
    replaced("/tag/v1.0.5", std::string("/tag/v1.0.5") + '\0' + "evil");
    reject(std::string(1025, 'x'));
    assert(!parseUpdateManifest(full, nullptr));
    assert(releaseVersion(0xffffffff) == "255.255.255.255");
    assert(releaseVersion(0x01000500) == "1.0.5");
    assert(releaseVersion(0x01000501) == "1.0.5.1");
    // Deterministic malformed-input stress run; sanitizer builds check bounds/overflow.
    std::mt19937 rng(42);
    for (int i = 0; i < 50000; ++i) {
        std::string input(rng() % 1100, '\0');
        for (char &c : input) c = static_cast<char>(rng() % 256);
        ReleaseManifest value;
        const bool accepted = parseUpdateManifest(input, &value);
        assert(!accepted);
    }
    std::cout << "Update manifest tests passed (including 50,000 malformed inputs).\n";
}
