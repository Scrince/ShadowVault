#pragma once

#include <sv/container/types.hpp>
#include <sv/result.hpp>

namespace sv::container {

[[nodiscard]] Result<ContainerLayout> compute_layout(const CreateLayoutParams& params) noexcept;

[[nodiscard]] std::uint64_t tail_superblock_offset(std::uint64_t container_size) noexcept;

[[nodiscard]] bool validate_layout(const Superblock& sb) noexcept;

} 