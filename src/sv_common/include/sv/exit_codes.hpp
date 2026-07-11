#pragma once

#include <cstdint>

namespace sv {

/// CLI process exit codes (see docs/cli-spec.md).
namespace exit_codes {

constexpr int success = 0;
constexpr int general_error = 1;
constexpr int usage_error = 2;
constexpr int wrong_credentials = 3;
constexpr int corrupt_container = 4;
constexpr int hidden_volume_protected = 5;
constexpr int vault_locked = 6;
constexpr int io_error = 7;
constexpr int manifest_full = 8;

} // namespace exit_codes

} // namespace sv