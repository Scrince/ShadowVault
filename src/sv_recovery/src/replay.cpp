#include <sv/recovery/replay.hpp>

namespace sv::recovery {

Result<void> replay_journal(Journal& journal, container::Superblock& superblock) noexcept {
    return journal.replay_if_dirty(&superblock);
}

} // namespace sv::recovery