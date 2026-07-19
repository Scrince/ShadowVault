#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace sv::keyfile {

constexpr std::size_t kMaxKeyfiles = 64;
constexpr std::size_t kMaxKeyfileReadSize = 512 * 1024;

using KeyfileHash = std::array<std::uint8_t, 32>;

struct KeyfileEntry {
    std::filesystem::path path;
    KeyfileHash hash {};
};

[[nodiscard]] Result<KeyfileHash> hash_keyfile_bytes(Span<const std::uint8_t> data) noexcept;
[[nodiscard]] Result<KeyfileEntry> read_keyfile(const std::filesystem::path& path);
[[nodiscard]] Result<std::vector<KeyfileEntry>> read_keyfiles(
    const std::vector<std::filesystem::path>& paths);

} 