#include "pba_fs_internal.h"

#include <sv/pba/pba_unlock_fs.h>

#include <sodium.h>

#include <string.h>

static int sv_pba_fs_sodium_ready(void) {
    if (sodium_init() < 0) {
        return 0;
    }
    return 1;
}

static struct sv_pba_fs_kdf_params sv_pba_fs_kdf_from_blob(const uint8_t* blob, size_t len) {
    struct sv_pba_fs_kdf_params params;
    params.m_cost_kib = SV_PBA_FS_DEFAULT_M_COST_KIB;
    params.t_cost = SV_PBA_FS_DEFAULT_T_COST;
    params.p_cost = SV_PBA_FS_DEFAULT_P_COST;

    if (blob == NULL || len < 12) {
        return params;
    }

    params.m_cost_kib = sv_pba_fs_read_le32(blob);
    params.t_cost = sv_pba_fs_read_le32(blob + 4);
    params.p_cost = sv_pba_fs_read_le32(blob + 8);
    return params;
}

struct sv_pba_fs_kdf_params sv_pba_fs_outer_kdf_params(const struct sv_pba_fs_superblock* sb) {
    size_t i;
    int has_hint = 0;

    if (sb == NULL) {
        struct sv_pba_fs_kdf_params defaults;
        defaults.m_cost_kib = SV_PBA_FS_DEFAULT_M_COST_KIB;
        defaults.t_cost = SV_PBA_FS_DEFAULT_T_COST;
        defaults.p_cost = SV_PBA_FS_DEFAULT_P_COST;
        return defaults;
    }

    for (i = 0; i < sizeof(sb->outer_kdf_id); ++i) {
        if (sb->outer_kdf_id[i] != 0) {
            has_hint = 1;
            break;
        }
    }

    if (!has_hint) {
        struct sv_pba_fs_kdf_params defaults;
        defaults.m_cost_kib = SV_PBA_FS_DEFAULT_M_COST_KIB;
        defaults.t_cost = SV_PBA_FS_DEFAULT_T_COST;
        defaults.p_cost = SV_PBA_FS_DEFAULT_P_COST;
        return defaults;
    }

    return sv_pba_fs_kdf_from_blob(sb->outer_kdf_id, sizeof(sb->outer_kdf_id));
}

static enum sv_pba_status sv_pba_fs_derive_kdf_secret(
    const uint8_t* password_utf8,
    size_t password_len,
    uint8_t secret_out[32]) {
    if (password_utf8 == NULL || password_len == 0) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (password_len < 12) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (crypto_hash_sha256(secret_out, password_utf8, password_len) != 0) {
        return SV_PBA_STATUS_PLATFORM_ERROR;
    }
    return SV_PBA_STATUS_OK;
}

enum sv_pba_status sv_pba_fs_derive_header_key(
    const uint8_t* password_utf8,
    size_t password_len,
    const struct sv_pba_fs_superblock* sb,
    const struct sv_pba_fs_kdf_params* kdf,
    uint8_t header_key_out[32]) {
    uint8_t kdf_secret[32];
    uint8_t derived[64];
    uint8_t salt16[crypto_pwhash_argon2id_SALTBYTES];
    enum sv_pba_status status;

    if (sb == NULL || kdf == NULL || header_key_out == NULL) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (!sv_pba_fs_sodium_ready()) {
        return SV_PBA_STATUS_PLATFORM_ERROR;
    }

    status = sv_pba_fs_derive_kdf_secret(password_utf8, password_len, kdf_secret);
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    memset(salt16, 0, sizeof(salt16));
    memcpy(salt16, sb->outer_salt, sizeof(salt16));

    if (crypto_pwhash_argon2id(
            derived,
            sizeof(derived),
            (const char*)kdf_secret,
            sizeof(kdf_secret),
            salt16,
            (unsigned long long)kdf->t_cost,
            (size_t)kdf->m_cost_kib * 1024U,
            crypto_pwhash_argon2id_ALG_ARGON2ID13)
        != 0) {
        sv_pba_fs_secure_zero(kdf_secret, sizeof(kdf_secret));
        sv_pba_fs_secure_zero(derived, sizeof(derived));
        return SV_PBA_STATUS_PLATFORM_ERROR;
    }

    memcpy(header_key_out, derived, 32);
    sv_pba_fs_secure_zero(kdf_secret, sizeof(kdf_secret));
    sv_pba_fs_secure_zero(derived, sizeof(derived));
    return SV_PBA_STATUS_OK;
}

enum sv_pba_status sv_pba_fs_header_key_confirm(
    const uint8_t header_key[32],
    uint8_t confirm_out[32]) {
    static const char k_confirm_info[] = "sv-confirm-v1";

    if (header_key == NULL || confirm_out == NULL) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (!sv_pba_fs_sodium_ready()) {
        return SV_PBA_STATUS_PLATFORM_ERROR;
    }

    if (crypto_auth_hmacsha256(
            confirm_out,
            (const unsigned char*)k_confirm_info,
            sizeof(k_confirm_info) - 1U,
            header_key)
        != 0) {
        return SV_PBA_STATUS_PLATFORM_ERROR;
    }
    return SV_PBA_STATUS_OK;
}

enum sv_pba_status sv_pba_fs_open_header_payload(
    const struct sv_pba_fs_header_sector* sector,
    const uint8_t header_key[32],
    const struct sv_pba_fs_superblock* sb,
    struct sv_pba_fs_header_payload* payload_out) {
    uint8_t aad[SV_PBA_FS_AAD_SIZE];
    uint8_t plaintext[SV_PBA_FS_HEADER_PAYLOAD_SIZE];
    uint8_t confirm[32];
    enum sv_pba_status status;

    if (sector == NULL || header_key == NULL || sb == NULL || payload_out == NULL) {
        return SV_PBA_STATUS_INVALID_ARGUMENT;
    }
    if (!sv_pba_fs_sodium_ready()) {
        return SV_PBA_STATUS_PLATFORM_ERROR;
    }

    status = sv_pba_fs_build_header_aad(
        sb,
        SV_PBA_FS_SLOT_PRIMARY_OUTER,
        SV_PBA_FS_SLOT_TYPE_OUTER,
        aad);
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    if (crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
            plaintext,
            NULL,
            sector->ciphertext,
            sizeof(sector->ciphertext),
            sector->tag,
            aad,
            sizeof(aad),
            sector->header_nonce,
            header_key)
        != 0) {
        sv_pba_fs_secure_zero(plaintext, sizeof(plaintext));
        return SV_PBA_STATUS_WRONG_CREDENTIALS;
    }

    status = sv_pba_fs_decode_header_payload(plaintext, sizeof(plaintext), payload_out);
    sv_pba_fs_secure_zero(plaintext, sizeof(plaintext));
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    status = sv_pba_fs_header_key_confirm(header_key, confirm);
    if (status != SV_PBA_STATUS_OK) {
        return status;
    }

    if (sodium_memcmp(payload_out->header_key_confirm, confirm, sizeof(confirm)) != 0) {
        sv_pba_fs_secure_zero(confirm, sizeof(confirm));
        return SV_PBA_STATUS_WRONG_CREDENTIALS;
    }

    sv_pba_fs_secure_zero(confirm, sizeof(confirm));
    return SV_PBA_STATUS_OK;
}