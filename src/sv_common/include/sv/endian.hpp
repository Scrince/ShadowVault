#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace sv {

inline void write_le16(std::uint8_t* out, std::uint16_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value & 0xFFU);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

inline void write_le32(std::uint8_t* out, std::uint32_t value) noexcept {
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFU);
    }
}

inline void write_le64(std::uint8_t* out, std::uint64_t value) noexcept {
    for (std::size_t i = 0; i < 8; ++i) {
        out[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFU);
    }
}

[[nodiscard]] inline std::uint16_t read_le16(const std::uint8_t* in) noexcept {
    return static_cast<std::uint16_t>(in[0]) | (static_cast<std::uint16_t>(in[1]) << 8);
}

[[nodiscard]] inline std::uint32_t read_le32(const std::uint8_t* in) noexcept {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(in[i]) << (8 * i);
    }
    return value;
}

[[nodiscard]] inline std::uint64_t read_le64(const std::uint8_t* in) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(in[i]) << (8 * i);
    }
    return value;
}

template<typename T>
void store_le(std::uint8_t* out, T value) noexcept {
    static_assert(std::is_integral_v<T>);
    if constexpr (sizeof(T) == 2) {
        write_le16(out, static_cast<std::uint16_t>(value));
    } else if constexpr (sizeof(T) == 4) {
        write_le32(out, static_cast<std::uint32_t>(value));
    } else if constexpr (sizeof(T) == 8) {
        write_le64(out, static_cast<std::uint64_t>(value));
    }
}

template<typename T>
[[nodiscard]] T load_le(const std::uint8_t* in) noexcept {
    static_assert(std::is_integral_v<T>);
    if constexpr (sizeof(T) == 2) {
        return static_cast<T>(read_le16(in));
    } else if constexpr (sizeof(T) == 4) {
        return static_cast<T>(read_le32(in));
    } else if constexpr (sizeof(T) == 8) {
        return static_cast<T>(read_le64(in));
    }
}

} 