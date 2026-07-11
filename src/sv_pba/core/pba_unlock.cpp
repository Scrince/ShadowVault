#include <sv/pba/pba_unlock.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/pba/pba_unlock_fs.h>

namespace sv::pba {

namespace {

Result<std::array<std::uint8_t, 32>> map_fs_status(
    const sv_pba_status status,
    const std::array<std::uint8_t, 32>& vmk) noexcept {
    switch (status) {
    case SV_PBA_STATUS_OK:
        return Result<std::array<std::uint8_t, 32>>::ok(vmk);
    case SV_PBA_STATUS_INVALID_ARGUMENT:
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::InvalidArgument, "invalid unlock request");
    case SV_PBA_STATUS_WRONG_CREDENTIALS:
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::WrongCredentials, "wrong credentials");
    case SV_PBA_STATUS_CORRUPT_CONTAINER:
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::CorruptContainer, "corrupt container");
    case SV_PBA_STATUS_PLATFORM_ERROR:
    default:
        return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::PlatformError, "platform error");
    }
}

} // namespace

Result<std::array<std::uint8_t, 32>> unlock_primary_header(const PbaUnlockRequest& request) noexcept {
    const auto view = inspect_header_sector(request.primary_header_sector);
    if (!view.magic_ok) {
        return Result<std::array<std::uint8_t, 32>>::err(
            ErrorCode::CorruptContainer,
            "invalid header envelope");
    }

    sv_pba_unlock_request_t fs_request {};
    fs_request.primary_header_sector = request.primary_header_sector.data();
    fs_request.primary_header_sector_len = request.primary_header_sector.size();
    fs_request.superblock_bytes = request.superblock_bytes.data();
    fs_request.superblock_bytes_len = request.superblock_bytes.size();
    fs_request.password_utf8 = request.credentials.password_utf8.data();
    fs_request.password_utf8_len = request.credentials.password_utf8.size();

    std::array<std::uint8_t, 32> vmk {};
    const auto status = sv_pba_unlock_primary_header(&fs_request, vmk.data());
    return map_fs_status(status, vmk);
}

Result<std::array<std::uint8_t, 32>> unlock_system_header(
    const Span<const std::uint8_t> primary_header_sector,
    const PbaCredentials& credentials) noexcept {
    (void)primary_header_sector;
    (void)credentials;
    return Result<std::array<std::uint8_t, 32>>::err(
        ErrorCode::InvalidArgument,
        "unlock_system_header requires superblock; use unlock_primary_header");
}

} // namespace sv::pba