#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include "../paths_detail.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace sv::platform::detail {

namespace {

std::filesystem::path path_from_windows_string(const std::wstring& value) {
    return std::filesystem::path(value);
}

Result<std::filesystem::path> platform_error(const char* context) {
    const DWORD error = GetLastError();
    return Result<std::filesystem::path>::err(
        ErrorCode::PlatformError,
        std::string(context) + " (Win32 error " + std::to_string(error) + ")");
}

} // namespace

Result<std::filesystem::path> query_executable_directory() {
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

    while (length == MAX_PATH) {
        buffer.resize(buffer.size() * 2, L'\0');
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    if (length == 0) {
        return platform_error("GetModuleFileNameW failed");
    }

    const auto exe_path = path_from_windows_string(std::wstring(buffer.data(), length));
    return Result<std::filesystem::path>::ok(exe_path.parent_path());
}

Result<std::filesystem::path> query_host_config_directory() {
    wchar_t* value = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &value);
    if (FAILED(hr) || value == nullptr) {
        auto fallback = query_executable_directory();
        if (!fallback.is_ok()) {
            return fallback;
        }
        return Result<std::filesystem::path>::ok(fallback.value() / "ShadowVault");
    }

    const auto path = path_from_windows_string(value);
    CoTaskMemFree(value);
    return Result<std::filesystem::path>::ok(path / "ShadowVault");
}

Result<std::filesystem::path> query_temp_directory() {
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    const DWORD length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0 || length > buffer.size()) {
        std::error_code ec;
        const auto temp = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return Result<std::filesystem::path>::err(ErrorCode::PlatformError, ec.message());
        }
        return Result<std::filesystem::path>::ok(temp);
    }

    return Result<std::filesystem::path>::ok(path_from_windows_string(std::wstring(buffer.data(), length)));
}

Result<bool> query_is_removable_drive(const std::filesystem::path& path) {
    const auto root = path.root_name().wstring();
    if (root.empty()) {
        return Result<bool>::ok(false);
    }

    const auto drive = root.back() == L'\\' ? root : root + L"\\";
    const UINT drive_type = GetDriveTypeW(drive.c_str());
    const bool removable =
        drive_type == DRIVE_REMOVABLE || drive_type == DRIVE_CDROM;
    return Result<bool>::ok(removable);
}

} // namespace sv::platform::detail

#endif