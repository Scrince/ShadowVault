#pragma once

#include <sv/result.hpp>

#include <string>

namespace sv::platform::detail {

[[nodiscard]] Result<std::string> read_line_no_echo();

} // namespace sv::platform::detail