#include <sv/vault/inode.hpp>

#include <sv/endian.hpp>

#include <algorithm>
#include <cstring>

namespace sv::vault {

namespace {

constexpr std::size_t kNameFieldSize = 256;

} // namespace

Result<void> encode_inode(const Inode& inode, Span<std::uint8_t> out) noexcept {
    if (out.size() < container::SVLT_INODE_SIZE) {
        return Result<void>::err(ErrorCode::InvalidArgument, "inode buffer too small");
    }
    if (inode.name_utf8.size() > 255) {
        return Result<void>::err(ErrorCode::InvalidArgument, "inode name too long");
    }

    std::memset(out.data(), 0, out.size());
    write_le16(out.data(), inode.inode_version);
    out[2] = static_cast<std::uint8_t>(inode.entry_type);
    out[3] = static_cast<std::uint8_t>(inode.name_utf8.size());
    std::memcpy(out.data() + 4, inode.name_utf8.data(), inode.name_utf8.size());
    write_le64(out.data() + 260, inode.parent_id);
    write_le64(out.data() + 268, inode.inode_id);
    write_le64(out.data() + 276, inode.size);
    write_le64(out.data() + 284, inode.mtime_unix);
    write_le32(out.data() + 292, inode.chunk_count);
    write_le32(out.data() + 296, inode.flags);
    write_le64(out.data() + 300, inode.first_chunk_id);
    std::memcpy(out.data() + 308, inode.base_nonce.data(), inode.base_nonce.size());
    write_le64(out.data() + 332, inode.data_generation);
    return Result<void>::ok();
}

Result<Inode> decode_inode(Span<const std::uint8_t> data) noexcept {
    if (data.size() < container::SVLT_INODE_SIZE) {
        return Result<Inode>::err(ErrorCode::CorruptContainer, "inode too short");
    }

    Inode inode {};
    inode.inode_version = read_le16(data.data());
    inode.entry_type = static_cast<container::InodeEntryType>(data[2]);
    const std::uint8_t name_len = data[3];
    if (name_len > 255) {
        return Result<Inode>::err(ErrorCode::CorruptContainer, "invalid inode name length");
    }
    inode.name_utf8.assign(reinterpret_cast<const char*>(data.data() + 4), name_len);
    inode.parent_id = read_le64(data.data() + 260);
    inode.inode_id = read_le64(data.data() + 268);
    inode.size = read_le64(data.data() + 276);
    inode.mtime_unix = read_le64(data.data() + 284);
    inode.chunk_count = read_le32(data.data() + 292);
    inode.flags = read_le32(data.data() + 296);
    inode.first_chunk_id = read_le64(data.data() + 300);
    std::memcpy(inode.base_nonce.data(), data.data() + 308, inode.base_nonce.size());
    if (data.size() >= 340) {
        inode.data_generation = read_le64(data.data() + 332);
    }
    return Result<Inode>::ok(inode);
}

bool inode_less(const Inode& a, const Inode& b) noexcept {
    if (a.parent_id != b.parent_id) {
        return a.parent_id < b.parent_id;
    }
    return a.name_utf8 < b.name_utf8;
}

namespace {

template<typename InodeRange>
auto find_inode_in_range(
    InodeRange& inodes,
    const std::uint64_t parent_id,
    const std::string_view name) {
    Inode probe {};
    probe.parent_id = parent_id;
    probe.name_utf8 = std::string(name);
    const auto it = std::lower_bound(inodes.begin(), inodes.end(), probe, inode_less);
    if (it != inodes.end() && it->parent_id == parent_id && it->name_utf8 == name) {
        return it;
    }
    return inodes.end();
}

} // namespace

std::vector<Inode>::iterator find_inode(
    std::vector<Inode>& inodes,
    const std::uint64_t parent_id,
    const std::string_view name) noexcept {
    return find_inode_in_range(inodes, parent_id, name);
}

std::vector<Inode>::const_iterator find_inode(
    const std::vector<Inode>& inodes,
    const std::uint64_t parent_id,
    const std::string_view name) noexcept {
    return find_inode_in_range(inodes, parent_id, name);
}

} // namespace sv::vault