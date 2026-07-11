#include <sv/pba/pba_header.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/container/types.hpp>

#include <cstring>

namespace sv::pba {

SystemHeaderView inspect_header_sector(const Span<const std::uint8_t> sector) noexcept {
    SystemHeaderView view {};
    if (sector.size() < container::SVLT_HEADER_SIZE) {
        return view;
    }

    if (std::memcmp(sector.data(), container::SVLT_MAGIC, sizeof(container::SVLT_MAGIC)) == 0) {
        view.magic_ok = true;
        if (sector.size() >= 6) {
            view.format_version = static_cast<std::uint16_t>(sector[4])
                | (static_cast<std::uint16_t>(sector[5]) << 8);
        }
        return view;
    }

    // Container-compatible encrypted outer header (no leading magic; CRC-validated slot).
    const auto decoded = container::decode_header_sector(sector);
    if (decoded.is_ok()) {
        view.magic_ok = true;
        view.format_version = 1;
    }
    return view;
}

} // namespace sv::pba