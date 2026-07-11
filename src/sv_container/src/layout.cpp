#include <sv/container/layout.hpp>

namespace sv::container {

namespace {

constexpr std::uint64_t kMinJournalSize = 64ULL * 1024;
constexpr std::uint64_t kMinManifestSize = 128ULL * 1024;
constexpr std::uint64_t kMaxJournalSize = 64ULL * 1024 * 1024;
constexpr std::uint64_t kMaxManifestSize = 128ULL * 1024 * 1024;

[[nodiscard]] std::uint64_t clamp_region_size(
    std::uint64_t container_size,
    std::uint64_t min_size,
    std::uint64_t max_size,
    std::uint64_t fraction_divisor) noexcept {
    const std::uint64_t scaled = container_size / fraction_divisor;
    if (scaled < min_size) {
        return min_size;
    }
    if (scaled > max_size) {
        return max_size;
    }
    return scaled;
}

} // namespace

std::uint64_t tail_superblock_offset(const std::uint64_t container_size) noexcept {
    return container_size - SVLT_SUPERBLOCK_SIZE;
}

Result<ContainerLayout> compute_layout(const CreateLayoutParams& params) noexcept {
    if (params.container_size < SVLT_JOURNAL_OFFSET + kMinJournalSize + kMinManifestSize + SVLT_SUPERBLOCK_SIZE) {
        return Result<ContainerLayout>::err(
            ErrorCode::InvalidArgument,
            "container_size too small for minimum journal and manifest regions");
    }

    ContainerLayout layout {};
    layout.container_size = params.container_size;
    layout.journal_offset = SVLT_JOURNAL_OFFSET;
    layout.journal_size = clamp_region_size(params.container_size, kMinJournalSize, kMaxJournalSize, 32);
    layout.manifest_offset = layout.journal_offset + layout.journal_size;
    layout.manifest_size = clamp_region_size(params.container_size, kMinManifestSize, kMaxManifestSize, 16);
    layout.data_offset = layout.manifest_offset + layout.manifest_size;
    layout.data_size = params.container_size - layout.data_offset - SVLT_SUPERBLOCK_SIZE;
    layout.tail_superblock_offset = tail_superblock_offset(params.container_size);

    if (layout.data_size == 0) {
        return Result<ContainerLayout>::err(ErrorCode::InvalidArgument, "data_size would be zero");
    }

    return Result<ContainerLayout>::ok(layout);
}

bool validate_layout(const Superblock& sb) noexcept {
    if (sb.magic[0] != SVLT_MAGIC[0] || sb.magic[1] != SVLT_MAGIC[1] || sb.magic[2] != SVLT_MAGIC[2]
        || sb.magic[3] != SVLT_MAGIC[3]) {
        return false;
    }
    if (sb.format_version != SVLT_FORMAT_VERSION) {
        return false;
    }
    if (sb.superblock_size != SVLT_SUPERBLOCK_SIZE) {
        return false;
    }
    if (sb.container_size < sb.data_offset + sb.data_size + SVLT_SUPERBLOCK_SIZE) {
        return false;
    }
    if (sb.container_size != sb.data_offset + sb.data_size + SVLT_SUPERBLOCK_SIZE) {
        return false;
    }
    const std::uint64_t tail = tail_superblock_offset(sb.container_size);
    if (tail != sb.data_offset + sb.data_size) {
        return false;
    }
    if (sb.journal_offset + sb.journal_size > sb.manifest_offset) {
        return false;
    }
    if (sb.manifest_offset + sb.manifest_size > sb.data_offset) {
        return false;
    }
    if (sb.primary_header_offset + sb.primary_header_size > sb.backup_header_offset) {
        return false;
    }
    if (sb.backup_header_offset + sb.backup_header_size > sb.hidden_header_offset) {
        return false;
    }
    if (sb.hidden_header_offset + sb.hidden_header_size > sb.journal_offset) {
        return false;
    }
    return true;
}

} // namespace sv::container