#include <sv/platform/file_io.hpp>

#include <sv/logger.hpp>

#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace sv::platform {

namespace {

Result<void> io_error(const std::string& context, const std::error_code& ec) {
    return Result<void>::err(ErrorCode::IoError, context + ": " + ec.message());
}

Result<void> io_error_message(const std::string& message) {
    return Result<void>::err(ErrorCode::IoError, message);
}

} 

Result<std::vector<std::uint8_t>> read_file(
    const std::filesystem::path& path,
    const std::size_t max_size) {
    if (path.empty()) {
        return Result<std::vector<std::uint8_t>>::err(
            ErrorCode::InvalidArgument,
            "path must not be empty");
    }

    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return Result<std::vector<std::uint8_t>>::err(ErrorCode::IoError, ec.message());
    }

    if (size > max_size) {
        return Result<std::vector<std::uint8_t>>::err(
            ErrorCode::InvalidArgument,
            "file exceeds max_size (" + std::to_string(size) + " > " + std::to_string(max_size) + ")");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::vector<std::uint8_t>>::err(
            ErrorCode::IoError,
            "failed to open file for reading");
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    if (!data.empty()) {
        input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!input) {
            return Result<std::vector<std::uint8_t>>::err(
                ErrorCode::IoError,
                "failed while reading file");
        }
    }

    return Result<std::vector<std::uint8_t>>::ok(std::move(data));
}

Result<void> write_file(
    const std::filesystem::path& path,
    const ConstByteSpan data,
    const bool create_dirs) {
    if (path.empty()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "path must not be empty");
    }

    if (create_dirs) {
        const auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return io_error("failed to create parent directories", ec);
            }
        }
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return io_error_message("failed to open file for writing");
    }

    if (!data.empty()) {
        output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!output) {
            return io_error_message("failed while writing file");
        }
    }

    return Result<void>::ok();
}

Result<std::uint64_t> file_size(const std::filesystem::path& path) {
    if (path.empty()) {
        return Result<std::uint64_t>::err(ErrorCode::InvalidArgument, "path must not be empty");
    }

    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return Result<std::uint64_t>::err(ErrorCode::IoError, ec.message());
    }
    return Result<std::uint64_t>::ok(size);
}

Result<void> preallocate_file(const std::filesystem::path& path, const std::uint64_t size) {
    if (path.empty()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "path must not be empty");
    }

#if defined(_WIN32)
    const auto native = path.native();
    HANDLE handle = CreateFileW(
        native.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return Result<void>::err(
            ErrorCode::IoError,
            "CreateFileW failed (Win32 error " + std::to_string(error) + ")");
    }

    LARGE_INTEGER length {};
    length.QuadPart = static_cast<LONGLONG>(size);
    if (SetFilePointerEx(handle, length, nullptr, FILE_BEGIN) == 0) {
        const DWORD error = GetLastError();
        CloseHandle(handle);
        return Result<void>::err(
            ErrorCode::IoError,
            "SetFilePointerEx failed (Win32 error " + std::to_string(error) + ")");
    }

    if (SetEndOfFile(handle) == 0) {
        const DWORD error = GetLastError();
        CloseHandle(handle);
        return Result<void>::err(
            ErrorCode::IoError,
            "SetEndOfFile failed (Win32 error " + std::to_string(error) + ")");
    }

    CloseHandle(handle);
    return Result<void>::ok();
#else
    const auto native = path.native();
    // Containers hold ciphertext; never create world-readable vault files.
    const int fd = ::open(native.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return Result<void>::err(ErrorCode::IoError, std::strerror(errno));
    }

    if (::fchmod(fd, 0600) != 0) {
        // Best-effort; continue if the filesystem rejects chmod (e.g. some network FS).
    }

    if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
        const int error = errno;
        ::close(fd);
        return Result<void>::err(ErrorCode::IoError, std::strerror(error));
    }

    ::close(fd);
    return Result<void>::ok();
#endif
}

Result<void> sync_file(const NativeFileHandle handle) {
    if (handle == kInvalidFileHandle) {
        return Result<void>::err(ErrorCode::InvalidArgument, "invalid file handle");
    }

#if defined(_WIN32)
    if (FlushFileBuffers(static_cast<HANDLE>(handle)) == 0) {
        log_warn("platform", "FlushFileBuffers failed; continuing best-effort");
        return Result<void>::err(
            ErrorCode::IoError,
            "FlushFileBuffers failed (Win32 error " + std::to_string(GetLastError()) + ")");
    }
    return Result<void>::ok();
#else
    if (::fsync(static_cast<int>(handle)) != 0) {
        log_warn("platform", "fsync failed; continuing best-effort");
        return Result<void>::err(ErrorCode::IoError, std::strerror(errno));
    }
    return Result<void>::ok();
#endif
}

} 