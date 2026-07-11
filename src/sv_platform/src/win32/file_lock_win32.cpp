#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../file_lock_detail.hpp"

#include <string>

namespace sv::platform::detail {

FileLockState::~FileLockState() {
    if (file_handle != INVALID_HANDLE_VALUE) {
        OVERLAPPED overlapped {};
        UnlockFileEx(
            static_cast<HANDLE>(file_handle),
            0,
            MAXDWORD,
            MAXDWORD,
            &overlapped);
        CloseHandle(static_cast<HANDLE>(file_handle));
        file_handle = INVALID_HANDLE_VALUE;
    }
}

Result<std::unique_ptr<FileLockState>> open_exclusive_lock(
    const std::filesystem::path& path) {
    const auto native = path.native();
    HANDLE handle = CreateFileW(
        native.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return Result<std::unique_ptr<FileLockState>>::err(
            ErrorCode::IoError,
            "CreateFileW failed (Win32 error " + std::to_string(error) + ")");
    }

    OVERLAPPED overlapped {};
    if (LockFileEx(
            handle,
            LOCKFILE_EXCLUSIVE_LOCK,
            0,
            MAXDWORD,
            MAXDWORD,
            &overlapped) == 0) {
        const DWORD error = GetLastError();
        CloseHandle(handle);
        return Result<std::unique_ptr<FileLockState>>::err(
            ErrorCode::IoError,
            "LockFileEx failed (Win32 error " + std::to_string(error) + ")");
    }

    auto state = std::make_unique<FileLockState>();
    state->path = path;
    state->file_handle = handle;
    return Result<std::unique_ptr<FileLockState>>::ok(std::move(state));
}

} // namespace sv::platform::detail

#endif