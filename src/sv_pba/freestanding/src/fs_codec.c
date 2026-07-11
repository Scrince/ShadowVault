#include "pba_fs_internal.h"

#include <sv/pba/pba_unlock_fs.h>

#include <string.h>

static int sv_pba_fs_magic_ok(const uint8_t* magic) {
    return magic[0] == SV_PBA_FS_MAGIC_0 && magic[1] == SV_PBA_FS_MAGIC_1
        && magic[2] == SV_PBA_FS_MAGIC_2 && magic[3] == SV_PBA_FS_MAGIC_3;
}

enum sv_pba_status sv_pba_fs_decode_superblock(
    const uint8_t* data,
    size_t len,
    struct sv_pba_fs_superblock* out) {
    uint32_t expected_crc;
    uint32_t actual_crc;

    if (out == NULL || data == NULL || len < SV_PBA_FS_SUPERBLOCK_SIZE) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }

    if (!sv_pba_fs_magic_ok(data)) {
        return SV_PBA_STATUS_CORRUPT_CONTAINER;
    }

    memset(out, 0, sizeof(*out));
    out->format_version = sv_pba_fs_read_le16(data + 0x04);
    memcpy(out->outer_salt, data + 0x68, sizeof(out->outer_salt));
    memcpy(out->hidden_salt, data + 0x88, sizeof(out->hidden_salt));
    memcpy(out->outer_kdf_id, data + 0xA8, sizeof(out->outer_kdf_id));

    actual_crc = sv_pba_fs_read_le32(data + 0xCC);
    expected_crc = sv_pba_fs_crc32_ieee(data, SV_PBA_FS_SUPERBLOCK_CRC_END);
    if (actual_crc != expected_crc) {
        return SV_PBA_STATUS_CORRUPT_CONTAINER;
    }

    return SV_PBA_STATUS_OK;
}

enum sv_pba_status sv_pba_fs_decode_header_sector(
    const uint8_t* data,
    size_t len,
    struct sv_pba_fs_header_sector* out) {
    uint32_t expected_crc;
    uint32_t actual_crc;

    if (out == NULL || data == NULL || len < SV_PBA_FS_HEADER_SIZE) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));
    memcpy(out->header_nonce, data, sizeof(out->header_nonce));
    memcpy(out->ciphertext, data + 24, sizeof(out->ciphertext));
    memcpy(out->tag, data + 3576, sizeof(out->tag));
    actual_crc = sv_pba_fs_read_le32(data + 4080);

    expected_crc = sv_pba_fs_crc32_ieee(data, SV_PBA_FS_HEADER_CRC_COVERAGE);
    if (actual_crc != expected_crc) {
        return SV_PBA_STATUS_CORRUPT_CONTAINER;
    }

    return SV_PBA_STATUS_OK;
}

enum sv_pba_status sv_pba_fs_decode_header_payload(
    const uint8_t* data,
    size_t len,
    struct sv_pba_fs_header_payload* out) {
    if (out == NULL || data == NULL || len < SV_PBA_FS_HEADER_PAYLOAD_SIZE) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));
    memcpy(out->volume_master_key, data + 0x04, sizeof(out->volume_master_key));
    memcpy(out->header_key_confirm, data + 0x24, sizeof(out->header_key_confirm));
    return SV_PBA_STATUS_OK;
}

enum sv_pba_status sv_pba_fs_build_header_aad(
    const struct sv_pba_fs_superblock* sb,
    uint16_t slot_id,
    uint8_t slot_type,
    uint8_t aad_out[SV_PBA_FS_AAD_SIZE]) {
    if (sb == NULL || aad_out == NULL) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }

    memset(aad_out, 0, SV_PBA_FS_AAD_SIZE);
    aad_out[0] = SV_PBA_FS_MAGIC_0;
    aad_out[1] = SV_PBA_FS_MAGIC_1;
    aad_out[2] = SV_PBA_FS_MAGIC_2;
    aad_out[3] = SV_PBA_FS_MAGIC_3;
    sv_pba_fs_write_le16(aad_out + 4, sb->format_version);
    sv_pba_fs_write_le16(aad_out + 6, slot_id);
    aad_out[8] = slot_type;
    memcpy(aad_out + 16, sb->outer_salt, sizeof(sb->outer_salt));
    memcpy(aad_out + 48, sb->hidden_salt, sizeof(sb->hidden_salt));
    return SV_PBA_STATUS_OK;
}