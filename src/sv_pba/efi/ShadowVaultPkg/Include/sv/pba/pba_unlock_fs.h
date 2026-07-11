#pragma once

/**
 * Freestanding C unlock API — host-tested with libsodium; EFI links same sources.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sv_pba_status {
    SV_PBA_STATUS_OK = 0,
    SV_PBA_STATUS_INVALID_ARGUMENT = 1,
    SV_PBA_STATUS_WRONG_CREDENTIALS = 2,
    SV_PBA_STATUS_CORRUPT_CONTAINER = 3,
    SV_PBA_STATUS_PLATFORM_ERROR = 4,
};

typedef struct sv_pba_unlock_request {
    const uint8_t* primary_header_sector;
    size_t primary_header_sector_len;
    const uint8_t* superblock_bytes;
    size_t superblock_bytes_len;
    const uint8_t* password_utf8;
    size_t password_utf8_len;
} sv_pba_unlock_request_t;

enum sv_pba_status sv_pba_unlock_primary_header(
    const sv_pba_unlock_request_t* request,
    uint8_t vmk_out[32]);

#ifdef __cplusplus
}
#endif