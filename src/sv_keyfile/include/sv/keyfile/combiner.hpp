#pragma once

#include <sv/keyfile/keyfile_format.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

#include <array>
#include <string_view>
#include <vector>

namespace sv::keyfile {


[[nodiscard]] Result<std::array<std::uint8_t, 32>> combine_credentials(
    std::string_view password_utf8,
    Span<const KeyfileHash> keyfile_hashes) noexcept;

} 