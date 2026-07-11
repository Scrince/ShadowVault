#include <sv/mount/virtual_block_device.hpp>

namespace sv::mount {

VirtualBlockDevice::VirtualBlockDevice(std::shared_ptr<vault::ChunkStore> chunk_store)
    : chunk_store_(std::move(chunk_store)) {}

Result<std::size_t> VirtualBlockDevice::read(std::uint64_t /*offset*/, Span<std::uint8_t> /*buf*/) {
    return Result<std::size_t>::err(
        ErrorCode::NotImplemented,
        "Virtual block device read requires a mount backend (WinFsp/FUSE); use svcli file commands in v1");
}

Result<std::size_t> VirtualBlockDevice::write(std::uint64_t /*offset*/, Span<const std::uint8_t> /*buf*/) {
    return Result<std::size_t>::err(
        ErrorCode::NotImplemented,
        "Virtual block device write requires a mount backend (WinFsp/FUSE); use svcli file commands in v1");
}

Result<void> VirtualBlockDevice::flush() {
    return Result<void>::ok();
}

std::uint64_t VirtualBlockDevice::size() const {
    return 0;
}

bool VirtualBlockDevice::read_only() const {
    return true;
}

std::uint64_t VirtualBlockDevice::block_index_for(std::uint64_t offset) const {
    (void)offset;
    return 0;
}

std::size_t VirtualBlockDevice::block_offset(std::uint64_t offset) const {
    return static_cast<std::size_t>(offset);
}

} // namespace sv::mount