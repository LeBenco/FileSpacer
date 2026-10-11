#include "UpdateManifest.h"
#include <vector>

namespace filespacer {

std::string releaseVersion(uint32_t version) {
    std::string text = std::to_string(version >> 24) + "."
        + std::to_string((version >> 16) & 255) + "."
        + std::to_string((version >> 8) & 255);
    if (version & 255) text += "." + std::to_string(version & 255);
    return text;
}

bool parseUpdateManifest(const std::string &text, ReleaseManifest *result) {
    if (!result || text.empty() || text.size() > 1024
            || text.find('\0') != std::string::npos || text.find('\r') != std::string::npos)
        return false;
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        const size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if ((lines.size() != 3 && lines.size() != 6) || lines[0] != "FSUP"
            || lines[1].size() != 8) return false;
    ReleaseManifest parsed;
    for (char c : lines[1]) {
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        parsed.version = (parsed.version << 4) | digit;
    }
    const std::string base = "https://github.com/LeBenco/FileSpacer/releases/";
    const std::string tag = "v" + releaseVersion(parsed.version);
    if (lines[2] != base + "tag/" + tag) return false;
    parsed.url = lines[2];
    if (lines.size() == 6) {
        if (lines[3] != base + "download/" + tag + "/FileSpacer-"
                + releaseVersion(parsed.version) + "-install.exe"
                || lines[4].size() != 64 || lines[5].empty() || lines[5].size() > 10)
            return false;
        parsed.installerUrl = lines[3];
        for (char c : lines[4]) {
            if (c >= 'A' && c <= 'F') c += 'a' - 'A';
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
            parsed.sha256 += c;
        }
        for (char c : lines[5]) {
            if (c < '0' || c > '9') return false;
            parsed.installerSize = parsed.installerSize * 10 + (c - '0');
            if (parsed.installerSize > MAX_INSTALLER_SIZE) return false;
        }
        if (!parsed.installerSize) return false;
    }
    *result = std::move(parsed);
    return true;
}

} // namespace filespacer
