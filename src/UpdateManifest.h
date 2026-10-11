#pragma once
#include <cstdint>
#include <string>

namespace filespacer {

struct ReleaseManifest {
    uint32_t version = 0;
    std::string url;
    std::string installerUrl;
    std::string sha256;
    uint64_t installerSize = 0;
};

constexpr uint64_t MAX_INSTALLER_SIZE = 256ULL * 1024 * 1024;
// Three-line legacy manifests remain readable; installation needs all six lines.
bool parseUpdateManifest(const std::string &text, ReleaseManifest *result);
std::string releaseVersion(uint32_t version);

} // namespace filespacer
