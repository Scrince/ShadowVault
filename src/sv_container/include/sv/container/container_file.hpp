#pragma once

#include <sv/container/types.hpp>
#include <sv/platform/file_io.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

#include <filesystem>
#include <vector>

namespace sv::container {

class ContainerFileStore {
public:
    [[nodiscard]] static Result<ContainerFileStore> open(const std::filesystem::path& path);
    [[nodiscard]] static Result<ContainerFileStore> create(
        const std::filesystem::path& path,
        std::uint64_t container_size);

    ContainerFileStore() noexcept = default;
    ~ContainerFileStore();

    ContainerFileStore(ContainerFileStore&& other) noexcept;
    ContainerFileStore& operator=(ContainerFileStore&& other) noexcept;

    ContainerFileStore(const ContainerFileStore&) = delete;
    ContainerFileStore& operator=(const ContainerFileStore&) = delete;

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] std::uint64_t file_size() const noexcept;

    [[nodiscard]] Result<std::vector<std::uint8_t>> read_region(std::uint64_t offset, std::size_t size);
    [[nodiscard]] Result<void> write_region(std::uint64_t offset, ConstByteSpan data);
    [[nodiscard]] Result<void> sync();

    [[nodiscard]] Result<Superblock> read_superblock_primary();
    [[nodiscard]] Result<Superblock> read_superblock_tail();
    [[nodiscard]] Result<void> write_superblock_both(const Superblock& sb);
    [[nodiscard]] Result<void> write_superblock_both_no_sync(const Superblock& sb);

    [[nodiscard]] Result<EncryptedHeaderSector> read_header_sector(std::uint64_t offset);
    [[nodiscard]] Result<void> write_header_sector(std::uint64_t offset, const EncryptedHeaderSector& sector);

private:
    explicit ContainerFileStore(platform::NativeFileHandle handle, std::filesystem::path path) noexcept;

    void close() noexcept;

    platform::NativeFileHandle handle_ = platform::kInvalidFileHandle;
    std::filesystem::path path_;
    std::uint64_t file_size_ = 0;
};

} 