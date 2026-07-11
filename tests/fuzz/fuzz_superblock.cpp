#include "fuzz_entry.hpp"

#include <sv/container/superblock.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
    std::array<std::uint8_t, sv::container::SVLT_SUPERBLOCK_SIZE> encoded {};
    if (size > 0) {
        const std::size_t copy = std::min(size, encoded.size());
        std::memcpy(encoded.data(), data, copy);
    }

    const auto decoded = sv::container::decode_superblock(encoded);
    if (decoded.is_ok()) {
        (void)sv::container::validate_superblock(decoded.value());
        (void)sv::container::compute_superblock_crc(encoded);
    }

    if (size > encoded.size()) {
        std::vector<std::uint8_t> oversized(size);
        std::memcpy(oversized.data(), data, size);
        (void)sv::container::decode_superblock(oversized);
    }

    return 0;
}