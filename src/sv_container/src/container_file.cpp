#include <sv/container/container_file.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/container/superblock.hpp>

#include <cstring>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sv::container {

namespace {

Result<void> io_error(const std::string& message) {
    return Result<void>::err(ErrorCode::IoError, message);
}

} // namespace

ContainerFileStore::ContainerFileStore(
    const platform::NativeFileHandle handle,
    std::filesystem::path path) noexcept
    : handle_(handle), path_(std::move(path)) {}

ContainerFileStore::~ContainerFileStore() {
    close();
}

ContainerFileStore::ContainerFileStore(ContainerFileStore&& other) noexcept
    : handle_(other.handle_), path_(std::move(other.path_)), file_size_(other.file_size_) {
    other.handle_ = platform::kInvalidFileHandle;
    other.file_size_ = 0;
}

ContainerFileStore& ContainerFileStore::operator=(ContainerFileStore&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        path_ = std::move(other.path_);
        file_size_ = other.file_size_;
        other.handle_ = platform::kInvalidFileHandle;
        other.file_size_ = 0;
    }
    return *this;
}

void ContainerFileStore::close() noexcept {
    if (handle_ != platform::kInvalidFileHandle) {
#if defined(_WIN32)
        CloseHandle(static_cast<HANDLE>(handle_));
#else
        ::close(static_cast<int>(handle_));
#endif
        handle_ = platform::kInvalidFileHandle;
    }
}

bool ContainerFileStore::is_open() const noexcept {
    return handle_ != platform::kInvalidFileHandle;
}

const std::filesystem::path& ContainerFileStore::path() const noexcept {
    return path_;
}

std::uint64_t ContainerFileStore::file_size() const noexcept {
    return file_size_;
}

Result<ContainerFileStore> ContainerFileStore::create(
    const std::filesystem::path& path,
    const std::uint64_t container_size) {
    const auto pre = platform::preallocate_file(path, container_size);
    if (!pre.is_ok()) {
        return Result<ContainerFileStore>::err(pre.error(), pre.message());
    }
    return open(path);
}

Result<ContainerFileStore> ContainerFileStore::open(const std::filesystem::path& path) {
    if (path.empty()) {
        return Result<ContainerFileStore>::err(ErrorCode::InvalidArgument, "path must not be empty");
    }

    const auto size_result = platform::file_size(path);
    if (!size_result.is_ok()) {
        return Result<ContainerFileStore>::err(size_result.error(), size_result.message());
    }

#if defined(_WIN32)
    const auto native = path.native();
    HANDLE handle = CreateFileW(
        native.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return Result<ContainerFileStore>::err(
            ErrorCode::IoError,
            "CreateFileW failed (Win32 error " + std::to_string(GetLastError()) + ")");
    }

    ContainerFileStore store(handle, path);
    store.file_size_ = size_result.value();
    return Result<ContainerFileStore>::ok(std::move(store));
#else
    const auto native = path.native();
    const int fd = ::open(native.c_str(), O_RDWR);
    if (fd < 0) {
        return Result<ContainerFileStore>::err(ErrorCode::IoError, std::strerror(errno));
    }

    ContainerFileStore store(fd, path);
    store.file_size_ = size_result.value();
    return Result<ContainerFileStore>::ok(std::move(store));
#endif
}

Result<std::vector<std::uint8_t>> ContainerFileStore::read_region(
    const std::uint64_t offset,
    const std::size_t size) {
    if (!is_open()) {
        return Result<std::vector<std::uint8_t>>::err(ErrorCode::IoError, "container not open");
    }
    if (offset + size > file_size_) {
        return Result<std::vector<std::uint8_t>>::err(ErrorCode::InvalidArgument, "read beyond container size");
    }

    std::vector<std::uint8_t> buffer(size);
    if (size == 0) {
        return Result<std::vector<std::uint8_t>>::ok(std::move(buffer));
    }

#if defined(_WIN32)
    LARGE_INTEGER pos {};
    pos.QuadPart = static_cast<LONGLONG>(offset);
    if (SetFilePointerEx(static_cast<HANDLE>(handle_), pos, nullptr, FILE_BEGIN) == 0) {
        return Result<std::vector<std::uint8_t>>::err(
            ErrorCode::IoError,
            "SetFilePointerEx failed (Win32 error " + std::to_string(GetLastError()) + ")");
    }

    DWORD bytes_read = 0;
    if (ReadFile(
            static_cast<HANDLE>(handle_),
            buffer.data(),
            static_cast<DWORD>(size),
            &bytes_read,
            nullptr)
        == 0) {
        return Result<std::vector<std::uint8_t>>::err(
            ErrorCode::IoError,
            "ReadFile failed (Win32 error " + std::to_string(GetLastError()) + ")");
    }
    if (bytes_read != size) {
        return Result<std::vector<std::uint8_t>>::err(ErrorCode::IoError, "short read");
    }
#else
    if (lseek(static_cast<int>(handle_), static_cast<off_t>(offset), SEEK_SET) < 0) {
        return Result<std::vector<std::uint8_t>>::err(ErrorCode::IoError, std::strerror(errno));
    }
    std::size_t total = 0;
    while (total < size) {
        const ssize_t n = ::read(
            static_cast<int>(handle_),
            buffer.data() + total,
            size - total);
        if (n < 0) {
            return Result<std::vector<std::uint8_t>>::err(ErrorCode::IoError, std::strerror(errno));
        }
        if (n == 0) {
            return Result<std::vector<std::uint8_t>>::err(ErrorCode::IoError, "unexpected EOF");
        }
        total += static_cast<std::size_t>(n);
    }
#endif

    return Result<std::vector<std::uint8_t>>::ok(std::move(buffer));
}

Result<void> ContainerFileStore::write_region(const std::uint64_t offset, const ConstByteSpan data) {
    if (!is_open()) {
        return io_error("container not open");
    }
    if (offset + data.size() > file_size_) {
        return Result<void>::err(ErrorCode::InvalidArgument, "write beyond container size");
    }
    if (data.empty()) {
        return Result<void>::ok();
    }

#if defined(_WIN32)
    LARGE_INTEGER pos {};
    pos.QuadPart = static_cast<LONGLONG>(offset);
    if (SetFilePointerEx(static_cast<HANDLE>(handle_), pos, nullptr, FILE_BEGIN) == 0) {
        return io_error("SetFilePointerEx failed (Win32 error " + std::to_string(GetLastError()) + ")");
    }

    DWORD bytes_written = 0;
    if (WriteFile(
            static_cast<HANDLE>(handle_),
            data.data(),
            static_cast<DWORD>(data.size()),
            &bytes_written,
            nullptr)
        == 0) {
        return io_error("WriteFile failed (Win32 error " + std::to_string(GetLastError()) + ")");
    }
    if (bytes_written != data.size()) {
        return io_error("short write");
    }
#else
    if (lseek(static_cast<int>(handle_), static_cast<off_t>(offset), SEEK_SET) < 0) {
        return io_error(std::strerror(errno));
    }
    std::size_t total = 0;
    while (total < data.size()) {
        const ssize_t n = ::write(
            static_cast<int>(handle_),
            reinterpret_cast<const char*>(data.data()) + total,
            data.size() - total);
        if (n < 0) {
            return io_error(std::strerror(errno));
        }
        total += static_cast<std::size_t>(n);
    }
#endif

    return Result<void>::ok();
}

Result<void> ContainerFileStore::sync() {
    if (!is_open()) {
        return io_error("container not open");
    }
    return platform::sync_file(handle_);
}

Result<Superblock> ContainerFileStore::read_superblock_primary() {
    const auto data = read_region(0, SVLT_SUPERBLOCK_SIZE);
    if (!data.is_ok()) {
        return Result<Superblock>::err(data.error(), data.message());
    }
    return decode_superblock(data.value());
}

Result<Superblock> ContainerFileStore::read_superblock_tail() {
    if (file_size_ < SVLT_SUPERBLOCK_SIZE) {
        return Result<Superblock>::err(ErrorCode::CorruptContainer, "container too small for tail superblock");
    }
    const auto data = read_region(file_size_ - SVLT_SUPERBLOCK_SIZE, SVLT_SUPERBLOCK_SIZE);
    if (!data.is_ok()) {
        return Result<Superblock>::err(data.error(), data.message());
    }
    return decode_superblock(data.value());
}

Result<void> ContainerFileStore::write_superblock_both_no_sync(const Superblock& sb) {
    std::array<std::uint8_t, SVLT_SUPERBLOCK_SIZE> encoded {};
    const auto enc = encode_superblock(sb, encoded);
    if (!enc.is_ok()) {
        return enc;
    }

    const auto primary = write_region(0, as_bytes(encoded));
    if (!primary.is_ok()) {
        return primary;
    }
    return write_region(file_size_ - SVLT_SUPERBLOCK_SIZE, as_bytes(encoded));
}

Result<void> ContainerFileStore::write_superblock_both(const Superblock& sb) {
    const auto written = write_superblock_both_no_sync(sb);
    if (!written.is_ok()) {
        return written;
    }
    return sync();
}

Result<EncryptedHeaderSector> ContainerFileStore::read_header_sector(const std::uint64_t offset) {
    const auto data = read_region(offset, SVLT_HEADER_SIZE);
    if (!data.is_ok()) {
        return Result<EncryptedHeaderSector>::err(data.error(), data.message());
    }
    return decode_header_sector(data.value());
}

Result<void> ContainerFileStore::write_header_sector(
    const std::uint64_t offset,
    const EncryptedHeaderSector& sector) {
    std::array<std::uint8_t, SVLT_HEADER_SIZE> encoded {};
    const auto enc = encode_header_sector(sector, encoded);
    if (!enc.is_ok()) {
        return enc;
    }
    return write_region(offset, as_bytes(encoded));
}

} // namespace sv::container