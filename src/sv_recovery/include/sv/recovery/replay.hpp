#pragma once

#include <sv/recovery/journal.hpp>
#include <sv/result.hpp>

namespace sv::recovery {

[[nodiscard]] Result<void> replay_journal(Journal& journal, container::Superblock& superblock) noexcept;

} 