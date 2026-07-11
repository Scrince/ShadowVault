#include "fuzz_entry.hpp"

#include <sv/container/header_codec.hpp>

#include <algorithm>
#include <array>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) {
    std::array<std::uint8_t, sv::container::SVLT_HEADER_SIZE> sector_bytes {};
    if (size > 0) {
        const std::size_t copy = std::min(size, sector_bytes.size());
        std::memcpy(sector_bytes.data(), data, copy);
    }

    const auto sector = sv::container::decode_header_sector(sector_bytes);
    if (sector.is_ok()) {
        (void)sv::container::decode_header_payload(sector.value().ciphertext);
    }

    if (size >= sv::container::SVLT_HEADER_PAYLOAD_SIZE) {
        (void)sv::container::decode_header_payload(
            sv::Span<const std::uint8_t>(data, sv::container::SVLT_HEADER_PAYLOAD_SIZE));
    }

    return 0;
}