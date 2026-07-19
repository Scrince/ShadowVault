#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace sv {


template<typename T>
using Span = std::span<T>;

using ByteSpan = Span<std::byte>;
using ConstByteSpan = Span<const std::byte>;

inline ConstByteSpan as_bytes(Span<const std::uint8_t> data) noexcept {
    return ConstByteSpan(reinterpret_cast<const std::byte*>(data.data()), data.size());
}

inline ByteSpan as_writable_bytes(Span<std::uint8_t> data) noexcept {
    return ByteSpan(reinterpret_cast<std::byte*>(data.data()), data.size());
}

} 