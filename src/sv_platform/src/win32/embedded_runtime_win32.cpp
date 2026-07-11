#include <sv/platform/embedded_runtime.hpp>
#include <sv/platform/paths.hpp>

#if defined(_WIN32) && defined(SV_EMBED_WINFSP)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace sv::platform {

namespace {

constexpr int kResWinfspDll = 101;
constexpr int kResPortableIni = 102;

bool write_file_if_changed(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        if (std::filesystem::file_size(path, ec) == data.size()) {
            return true;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return output.good();
}

bool extract_resource(const int resource_id, std::vector<std::uint8_t>& out) {
    out.clear();
    const HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr) {
        return false;
    }

    const HRSRC resource = FindResourceW(
        module,
        MAKEINTRESOURCEW(resource_id),
        reinterpret_cast<LPCWSTR>(RT_RCDATA));
    if (resource == nullptr) {
        return false;
    }

    const HGLOBAL loaded = LoadResource(module, resource);
    if (loaded == nullptr) {
        return false;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(LockResource(loaded));
    const DWORD size = SizeofResource(module, resource);
    if (bytes == nullptr || size == 0) {
        return false;
    }

    out.assign(bytes, bytes + size);
    return true;
}

std::filesystem::path winfsp_cache_directory() {
    wchar_t temp[MAX_PATH + 1] {};
    const DWORD len = GetTempPathW(MAX_PATH, temp);
    if (len == 0 || len > MAX_PATH) {
        return {};
    }
    return std::filesystem::path(temp) / "ShadowVault" / "winfsp";
}

void ensure_usb_data_dirs(const std::filesystem::path& exe_dir) {
    std::error_code ec;
    std::filesystem::create_directories(exe_dir / "vaults", ec);
    std::filesystem::create_directories(exe_dir / "keyfiles", ec);
    std::filesystem::create_directories(exe_dir / ".shadowvault" / "temp", ec);
}

} // namespace

void bootstrap_embedded_runtime() noexcept {
    const auto exe_dir_result = executable_directory();
    if (!exe_dir_result.is_ok()) {
        return;
    }
    const auto& exe_dir = exe_dir_result.value();

    std::vector<std::uint8_t> dll_bytes;
    if (extract_resource(kResWinfspDll, dll_bytes)) {
        const auto cache_dir = winfsp_cache_directory();
        if (!cache_dir.empty()) {
            const auto dll_path = cache_dir / "winfsp-x64.dll";
            if (write_file_if_changed(dll_path, dll_bytes)) {
                SetDllDirectoryW(dll_path.parent_path().wstring().c_str());
                (void)LoadLibraryW(dll_path.wstring().c_str());
            }
        }
    }

    const auto ini_path = exe_dir / "shadowvault.ini";
    if (!std::filesystem::exists(ini_path)) {
        std::vector<std::uint8_t> ini_bytes;
        if (extract_resource(kResPortableIni, ini_bytes)) {
            (void)write_file_if_changed(ini_path, ini_bytes);
        }
    }

    ensure_usb_data_dirs(exe_dir);
}

} // namespace sv::platform

#else

namespace sv::platform {

void bootstrap_embedded_runtime() noexcept {}

} // namespace sv::platform

#endif