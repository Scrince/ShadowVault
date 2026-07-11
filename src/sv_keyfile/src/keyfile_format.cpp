#include <sv/keyfile/keyfile_format.hpp>

#include <sv/platform/file_io.hpp>

#include <sodium.h>

#include <algorithm>

namespace sv::keyfile {

Result<KeyfileHash> hash_keyfile_bytes(Span<const std::uint8_t> data) noexcept {
    KeyfileHash hash {};
    if (crypto_hash_sha256(hash.data(), data.data(), data.size()) != 0) {
        return Result<KeyfileHash>::err(ErrorCode::PlatformError, "sha256 hash failed");
    }
    return Result<KeyfileHash>::ok(hash);
}

Result<KeyfileEntry> read_keyfile(const std::filesystem::path& path) {
    const auto data = platform::read_file(path, kMaxKeyfileReadSize);
    if (!data.is_ok()) {
        return Result<KeyfileEntry>::err(data.error(), data.message());
    }

    const auto hash = hash_keyfile_bytes(data.value());
    if (!hash.is_ok()) {
        return Result<KeyfileEntry>::err(hash.error(), hash.message());
    }

    KeyfileEntry entry {};
    entry.path = path;
    entry.hash = hash.value();
    return Result<KeyfileEntry>::ok(std::move(entry));
}

Result<std::vector<KeyfileEntry>> read_keyfiles(const std::vector<std::filesystem::path>& paths) {
    if (paths.size() > kMaxKeyfiles) {
        return Result<std::vector<KeyfileEntry>>::err(
            ErrorCode::InvalidArgument,
            "too many keyfiles (max " + std::to_string(kMaxKeyfiles) + ")");
    }

    std::vector<KeyfileEntry> entries;
    entries.reserve(paths.size());
    for (const auto& path : paths) {
        const auto entry = read_keyfile(path);
        if (!entry.is_ok()) {
            return Result<std::vector<KeyfileEntry>>::err(entry.error(), entry.message());
        }
        entries.push_back(entry.value());
    }

    std::sort(entries.begin(), entries.end(), [](const KeyfileEntry& a, const KeyfileEntry& b) {
        return a.path.lexically_normal().generic_string() < b.path.lexically_normal().generic_string();
    });

    return Result<std::vector<KeyfileEntry>>::ok(std::move(entries));
}

} // namespace sv::keyfile