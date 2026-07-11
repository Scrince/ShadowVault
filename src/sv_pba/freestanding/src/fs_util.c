#include "pba_fs_internal.h"

#include <string.h>

uint16_t sv_pba_fs_read_le16(const uint8_t* in) {
    return (uint16_t)in[0] | ((uint16_t)in[1] << 8);
}

uint32_t sv_pba_fs_read_le32(const uint8_t* in) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value |= (uint32_t)in[i] << (8U * (uint32_t)i);
    }
    return value;
}

uint64_t sv_pba_fs_read_le64(const uint8_t* in) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= (uint64_t)in[i] << (8U * (uint32_t)i);
    }
    return value;
}

void sv_pba_fs_write_le16(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)((value >> 8) & 0xFFU);
}

void sv_pba_fs_write_le32(uint8_t* out, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        out[i] = (uint8_t)((value >> (8U * (uint32_t)i)) & 0xFFU);
    }
}

uint32_t sv_pba_fs_crc32_ieee(const uint8_t* data, size_t len) {
    static const uint32_t k_polynomial = 0xEDB88320U;
    uint32_t crc = 0xFFFFFFFFU;

    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1) ^ (k_polynomial & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

void sv_pba_fs_secure_zero(void* ptr, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len-- > 0) {
        *p++ = 0;
    }
}