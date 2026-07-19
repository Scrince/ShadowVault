#include <sv/mount/svfs.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>

namespace sv::mount {

namespace {

constexpr std::uint64_t kRootMtime = 1;

} 

int result_to_errno(const Result<void>& result) noexcept {
    if (result.is_ok()) {
        return 0;
    }
    switch (result.error()) {
    case ErrorCode::InvalidArgument:
        return EINVAL;
    case ErrorCode::HiddenVolumeProtected:
        return ENOSPC;
    case ErrorCode::VaultLocked:
        return EBUSY;
    case ErrorCode::ManifestFull:
        return ENOSPC;
    case ErrorCode::IoError:
        return EIO;
    default:
        return EIO;
    }
}

int result_to_errno(const Result<std::size_t>& result) noexcept {
    if (result.is_ok()) {
        return 0;
    }
    return result_to_errno(Result<void>::err(result.error(), result.message()));
}

SvFs::SvFs(
    vault::VaultEngine& engine,
    vault::UnlockedVault& session,
    const MountOptions options,
    IoTouchCallback on_io)
    : engine_(engine)
    , session_(session)
    , options_(options)
    , on_io_(std::move(on_io)) {
    if (options_.read_only) {
        session_.read_only = true;
    }
    if (options_.protect_hidden) {
        session_.protect_hidden = true;
    }
}

std::filesystem::path SvFs::to_vault_path(const std::filesystem::path& fuse_path) const {
    if (fuse_path.empty() || fuse_path == "/" || fuse_path == "\\" || fuse_path == ".") {
        return std::filesystem::path("/");
    }
    auto normalized = fuse_path.generic_string();
    while (!normalized.empty() && (normalized.front() == '/' || normalized.front() == '\\')) {
        normalized.erase(normalized.begin());
    }
    if (normalized.empty()) {
        return std::filesystem::path("/");
    }
    return std::filesystem::path("/") / normalized;
}

void SvFs::touch_io() const {
    if (on_io_) {
        on_io_();
    }
}

Result<SvFsAttr> SvFs::getattr_vault_path(const std::filesystem::path& vault_path) const {
    if (vault_path == "/" || vault_path.empty()) {
        SvFsAttr attr {};
        attr.is_directory = true;
        attr.mtime_unix = kRootMtime;
        return Result<SvFsAttr>::ok(attr);
    }

    const auto parent = vault_path.parent_path();
    const auto name = vault_path.filename().generic_string();
    const auto entries = engine_.list(session_, parent.empty() ? std::filesystem::path("/") : parent);
    if (!entries.is_ok()) {
        return Result<SvFsAttr>::err(entries.error(), entries.message());
    }

    for (const auto& entry : entries.value()) {
        if (entry.name == name) {
            SvFsAttr attr {};
            attr.is_directory = entry.type == "dir";
            attr.size = entry.size;
            attr.mtime_unix = entry.mtime;
            return Result<SvFsAttr>::ok(attr);
        }
    }

    return Result<SvFsAttr>::err(ErrorCode::InvalidArgument, "path not found");
}

Result<SvFsAttr> SvFs::getattr(const std::filesystem::path& fuse_path) const {
    std::lock_guard lock(mutex_);
    touch_io();
    return getattr_vault_path(to_vault_path(fuse_path));
}

Result<std::vector<vault::VaultEntry>> SvFs::readdir(const std::filesystem::path& fuse_path) const {
    std::lock_guard lock(mutex_);
    touch_io();
    return engine_.list(session_, to_vault_path(fuse_path));
}

Result<int> SvFs::open(const std::filesystem::path& fuse_path, const bool write_requested) {
    if (write_requested && options_.read_only) {
        return Result<int>::err(ErrorCode::InvalidArgument, "read-only mount");
    }

    std::lock_guard lock(mutex_);
    touch_io();

    const auto vault_path = to_vault_path(fuse_path);
    const auto attr = getattr_vault_path(vault_path);
    if (!attr.is_ok()) {
        return Result<int>::err(attr.error(), attr.message());
    }
    if (attr.value().is_directory) {
        return Result<int>::err(ErrorCode::InvalidArgument, "is a directory");
    }

    auto data = engine_.get(session_, vault_path);
    if (!data.is_ok()) {
        return Result<int>::err(data.error(), data.message());
    }

    const int handle = next_handle_++;
    open_handles_.emplace(handle, OpenHandle {
        .vault_path = vault_path,
        .data = std::move(data.value()),
        .dirty = false,
    });
    return Result<int>::ok(handle);
}

Result<void> SvFs::release(const int handle) {
    std::lock_guard lock(mutex_);
    const auto it = open_handles_.find(handle);
    if (it == open_handles_.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "invalid handle");
    }

    if (it->second.dirty && !options_.read_only) {
        const auto put = engine_.put(
            session_,
            it->second.vault_path,
            Span<const std::uint8_t>(it->second.data.data(), it->second.data.size()),
            true);
        if (!put.is_ok()) {
            open_handles_.erase(it);
            return put;
        }
    }

    open_handles_.erase(it);
    return Result<void>::ok();
}

Result<std::size_t> SvFs::read(
    const int handle,
    const std::uint64_t offset,
    const Span<std::uint8_t> buffer) {
    std::lock_guard lock(mutex_);
    touch_io();

    const auto it = open_handles_.find(handle);
    if (it == open_handles_.end()) {
        return Result<std::size_t>::err(ErrorCode::InvalidArgument, "invalid handle");
    }

    if (offset >= it->second.data.size()) {
        return Result<std::size_t>::ok(0);
    }

    const auto available = it->second.data.size() - static_cast<std::size_t>(offset);
    const auto to_copy = std::min(buffer.size(), available);
    std::memcpy(buffer.data(), it->second.data.data() + offset, to_copy);
    return Result<std::size_t>::ok(to_copy);
}

Result<std::size_t> SvFs::write(
    const int handle,
    const std::uint64_t offset,
    const Span<const std::uint8_t> buffer) {
    if (options_.read_only) {
        return Result<std::size_t>::err(ErrorCode::InvalidArgument, "read-only mount");
    }

    std::lock_guard lock(mutex_);
    touch_io();

    const auto it = open_handles_.find(handle);
    if (it == open_handles_.end()) {
        return Result<std::size_t>::err(ErrorCode::InvalidArgument, "invalid handle");
    }

    if (buffer.size() > 0 && offset > std::numeric_limits<std::uint64_t>::max() - buffer.size()) {
        return Result<std::size_t>::err(ErrorCode::InvalidArgument, "write offset overflow");
    }
    const auto end = offset + buffer.size();
    if (end > it->second.data.size()) {
        it->second.data.resize(static_cast<std::size_t>(end));
    }
    std::memcpy(it->second.data.data() + offset, buffer.data(), buffer.size());
    it->second.dirty = true;

    
    const auto put = engine_.put(
        session_,
        it->second.vault_path,
        Span<const std::uint8_t>(it->second.data.data(), it->second.data.size()),
        true);
    if (!put.is_ok()) {
        return Result<std::size_t>::err(put.error(), put.message());
    }
    it->second.dirty = false;
    return Result<std::size_t>::ok(buffer.size());
}

Result<void> SvFs::flush(const int handle) {
    std::lock_guard lock(mutex_);
    const auto it = open_handles_.find(handle);
    if (it == open_handles_.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "invalid handle");
    }
    if (!it->second.dirty || options_.read_only) {
        return Result<void>::ok();
    }

    const auto put = engine_.put(
        session_,
        it->second.vault_path,
        Span<const std::uint8_t>(it->second.data.data(), it->second.data.size()),
        true);
    if (!put.is_ok()) {
        return put;
    }
    it->second.dirty = false;
    return Result<void>::ok();
}

Result<void> SvFs::create_file(const std::filesystem::path& fuse_path) {
    if (options_.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "read-only mount");
    }
    std::lock_guard lock(mutex_);
    touch_io();
    return engine_.put(session_, to_vault_path(fuse_path), Span<const std::uint8_t> {}, false);
}

Result<void> SvFs::mkdir(const std::filesystem::path& fuse_path) {
    if (options_.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "read-only mount");
    }
    std::lock_guard lock(mutex_);
    touch_io();
    return engine_.mkdir(session_, to_vault_path(fuse_path));
}

Result<void> SvFs::unlink(const std::filesystem::path& fuse_path) {
    if (options_.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "read-only mount");
    }
    std::lock_guard lock(mutex_);
    touch_io();
    return engine_.remove(session_, to_vault_path(fuse_path));
}

Result<void> SvFs::rmdir(const std::filesystem::path& fuse_path) {
    return unlink(fuse_path);
}

Result<void> SvFs::rename(
    const std::filesystem::path& from_path,
    const std::filesystem::path& to_path) {
    if (options_.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "read-only mount");
    }
    std::lock_guard lock(mutex_);
    touch_io();
    return engine_.rename(session_, to_vault_path(from_path), to_vault_path(to_path));
}

Result<void> SvFs::truncate(const std::filesystem::path& fuse_path, const std::uint64_t size) {
    if (options_.read_only) {
        return Result<void>::err(ErrorCode::InvalidArgument, "read-only mount");
    }

    std::lock_guard lock(mutex_);
    touch_io();

    const auto vault_path = to_vault_path(fuse_path);
    auto data = engine_.get(session_, vault_path);
    if (!data.is_ok()) {
        return Result<void>::err(data.error(), data.message());
    }

    data.value().resize(static_cast<std::size_t>(size));
    return engine_.put(
        session_,
        vault_path,
        Span<const std::uint8_t>(data.value().data(), data.value().size()),
        true);
}

} 
