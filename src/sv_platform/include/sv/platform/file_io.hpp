#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <vector>

namespace sv::platform {

#if defined(_WIN32)
using NativeFileHandle = void*;
inline const NativeFileHandle kInvalidFileHandle = reinterpret_cast<NativeFileHandle>(static_cast<intptr_t>(-1));
#else
using NativeFileHandle = int;
inline constexpr NativeFileHandle kInvalidFileHandle = -1;
#endif

[[nodiscard]] Result<std::vector<std::uint8_t>> read_file(
    const std::filesystem::path& path,
    std::size_t max_size);

[[nodiscard]] Result<void> write_file(
    const std::filesystem::path& path,
    ConstByteSpan data,
    bool create_dirs = false);

[[nodiscard]] Result<std::uint64_t> file_size(const std::filesystem::path& path);

[[nodiscard]] Result<void> preallocate_file(const std::filesystem::path& path, std::uint64_t size);


[[nodiscard]] Result<void> sync_file(NativeFileHandle handle);

} 