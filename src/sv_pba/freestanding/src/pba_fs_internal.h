#pragma once

#include <stddef.h>
#include <stdint.h>

#define SV_PBA_FS_MAGIC_0 'S'
#define SV_PBA_FS_MAGIC_1 'V'
#define SV_PBA_FS_MAGIC_2 'L'
#define SV_PBA_FS_MAGIC_3 'T'

#define SV_PBA_FS_HEADER_SIZE           4096U
#define SV_PBA_FS_SUPERBLOCK_SIZE       512U
#define SV_PBA_FS_HEADER_PAYLOAD_SIZE   3552U
#define SV_PBA_FS_HEADER_NONCE_SIZE     24U
#define SV_PBA_FS_HEADER_CIPHERTEXT_SIZE 3552U
#define SV_PBA_FS_AAD_SIZE              80U
#define SV_PBA_FS_HEADER_CRC_COVERAGE   4080U
#define SV_PBA_FS_SUPERBLOCK_CRC_END    0xCCU
#define SV_PBA_FS_KDF_BLOB_SIZE         64U

#define SV_PBA_FS_SLOT_PRIMARY_OUTER    1U
#define SV_PBA_FS_SLOT_TYPE_OUTER       1U

#define SV_PBA_FS_DEFAULT_M_COST_KIB    262144U
#define SV_PBA_FS_DEFAULT_T_COST        3U
#define SV_PBA_FS_DEFAULT_P_COST        1U

/* Match host-side outer KDF clamps (key_manager.cpp). */
#define SV_PBA_FS_MIN_M_COST_KIB        8192U
#define SV_PBA_FS_MAX_M_COST_KIB        1048576U
#define SV_PBA_FS_MIN_T_COST            1U
#define SV_PBA_FS_MAX_T_COST            10U
#define SV_PBA_FS_MIN_P_COST            1U
#define SV_PBA_FS_MAX_P_COST            4U

enum sv_pba_status;

struct sv_pba_fs_kdf_params {
    uint32_t m_cost_kib;
    uint32_t t_cost;
    uint32_t p_cost;
};

struct sv_pba_fs_superblock {
    uint16_t format_version;
    uint8_t outer_salt[32];
    uint8_t hidden_salt[32];
    uint8_t outer_kdf_id[16];
};

struct sv_pba_fs_header_sector {
    uint8_t header_nonce[SV_PBA_FS_HEADER_NONCE_SIZE];
    uint8_t ciphertext[SV_PBA_FS_HEADER_CIPHERTEXT_SIZE];
    uint8_t tag[16];
};

struct sv_pba_fs_header_payload {
    uint8_t volume_master_key[32];
    uint8_t header_key_confirm[32];
};

uint16_t sv_pba_fs_read_le16(const uint8_t* in);
uint32_t sv_pba_fs_read_le32(const uint8_t* in);
uint64_t sv_pba_fs_read_le64(const uint8_t* in);
void sv_pba_fs_write_le16(uint8_t* out, uint16_t value);
void sv_pba_fs_write_le32(uint8_t* out, uint32_t value);

uint32_t sv_pba_fs_crc32_ieee(const uint8_t* data, size_t len);
void sv_pba_fs_secure_zero(void* ptr, size_t len);

enum sv_pba_status sv_pba_fs_decode_superblock(
    const uint8_t* data,
    size_t len,
    struct sv_pba_fs_superblock* out);

enum sv_pba_status sv_pba_fs_decode_header_sector(
    const uint8_t* data,
    size_t len,
    struct sv_pba_fs_header_sector* out);

enum sv_pba_status sv_pba_fs_decode_header_payload(
    const uint8_t* data,
    size_t len,
    struct sv_pba_fs_header_payload* out);

enum sv_pba_status sv_pba_fs_build_header_aad(
    const struct sv_pba_fs_superblock* sb,
    uint16_t slot_id,
    uint8_t slot_type,
    uint8_t aad_out[SV_PBA_FS_AAD_SIZE]);

struct sv_pba_fs_kdf_params sv_pba_fs_outer_kdf_params(const struct sv_pba_fs_superblock* sb);

enum sv_pba_status sv_pba_fs_derive_header_key(
    const uint8_t* password_utf8,
    size_t password_len,
    const struct sv_pba_fs_superblock* sb,
    const struct sv_pba_fs_kdf_params* kdf,
    uint8_t header_key_out[32]);

enum sv_pba_status sv_pba_fs_header_key_confirm(
    const uint8_t header_key[32],
    uint8_t confirm_out[32]);

enum sv_pba_status sv_pba_fs_open_header_payload(
    const struct sv_pba_fs_header_sector* sector,
    const uint8_t header_key[32],
    const struct sv_pba_fs_superblock* sb,
    struct sv_pba_fs_header_payload* payload_out);