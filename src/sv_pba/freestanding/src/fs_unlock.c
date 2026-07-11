#include <sv/pba/pba_unlock_fs.h>

#include "pba_fs_internal.h"

#include <string.h>

enum sv_pba_status sv_pba_unlock_primary_header(
    const sv_pba_unlock_request_t* request,
    uint8_t vmk_out[32]) {
    struct sv_pba_fs_superblock sb;
    struct sv_pba_fs_header_sector sector;
    struct sv_pba_fs_header_payload payload;
    struct sv_pba_fs_kdf_params kdf;
    uint8_t header_key[32];
    enum sv_pba_status status;

    if (request == NULL || vmk_out == NULL) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (request->password_utf8 == NULL || request->password_utf8_len == 0) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (request->primary_header_sector == NULL
        || request->primary_header_sector_len < SV_PBA_FS_HEADER_SIZE) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (request->superblock_bytes == NULL
        || request->superblock_bytes_len < SV_PBA_FS_SUPERBLOCK_SIZE) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }

    status = sv_pba_fs_decode_superblock(
        request->superblock_bytes,
        request->superblock_bytes_len,
        &sb);
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    status = sv_pba_fs_decode_header_sector(
        request->primary_header_sector,
        request->primary_header_sector_len,
        &sector);
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    kdf = sv_pba_fs_outer_kdf_params(&sb);
    status = sv_pba_fs_derive_header_key(
        request->password_utf8,
        request->password_utf8_len,
        &sb,
        &kdf,
        header_key);
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    status = sv_pba_fs_open_header_payload(&sector, header_key, &sb, &payload);
    sv_pba_fs_secure_zero(header_key, sizeof(header_key));
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    memcpy(vmk_out, payload.volume_master_key, 32);
    sv_pba_fs_secure_zero(&payload, sizeof(payload));
    return SV_PBA_STATUS_OK;
}