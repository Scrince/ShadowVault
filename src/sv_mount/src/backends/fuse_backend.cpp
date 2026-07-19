#include <sv/mount/backends/fuse_backend.hpp>

#include <sv/mount/svfs.hpp>
#include <sv/vault/session_store.hpp>
#include <sv/vault/vault_engine.hpp>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(SV_HAVE_WINFSP)
#define FUSE_USE_VERSION 30
#include <fuse3/fuse.h>
#include <fuse3/fuse_common.h>
#elif defined(SV_HAVE_FUSE3)
#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#endif

namespace sv::mount {

namespace {

#if defined(SV_HAVE_WINFSP) || defined(SV_HAVE_FUSE3)

struct FuseMountState {
    std::filesystem::path mount_point;
    std::filesystem::path vault_path;
    MountOptions options;
    std::unique_ptr<vault::VaultEngine> engine;
    std::unique_ptr<SvFs> svfs;
    fuse* fuse_handle{nullptr};
    std::thread fuse_thread;
    std::atomic<bool> running{false};
};

std::mutex g_fuse_mutex;
std::unordered_map<std::string, std::unique_ptr<FuseMountState>> g_fuse_mounts;

FuseMountState* state_from_context() {
    const auto* context = fuse_get_context();
    if (context == nullptr || context->private_data == nullptr) {
        return nullptr;
    }
    return static_cast<FuseMountState*>(context->private_data);
}

void fill_stat(const SvFsAttr& attr, struct fuse_stat* stbuf, const bool read_only) {
    std::memset(stbuf, 0, sizeof(*stbuf));
    const auto file_mode = read_only ? 0444 : 0644;
    const auto dir_mode = read_only ? 0555 : 0755;
    stbuf->st_mode = attr.is_directory ? (S_IFDIR | dir_mode) : (S_IFREG | file_mode);
    stbuf->st_nlink = 1;
    stbuf->st_size = static_cast<fuse_off_t>(attr.size);
    stbuf->st_mtim.tv_sec = static_cast<decltype(stbuf->st_mtim.tv_sec)>(attr.mtime_unix);
    stbuf->st_atim.tv_sec = stbuf->st_mtim.tv_sec;
    stbuf->st_ctim.tv_sec = stbuf->st_mtim.tv_sec;
}

int svfs_getattr(const char* path, struct fuse_stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }

    const auto attr = state->svfs->getattr(path);
    if (!attr.is_ok()) {
        return -ENOENT;
    }
    fill_stat(attr.value(), stbuf, state->options.read_only);
    return 0;
}

int svfs_readdir(
    const char* path,
    void* buf,
    fuse_fill_dir_t filler,
    fuse_off_t offset,
    struct fuse_file_info* fi,
    enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }

    const auto entries = state->svfs->readdir(path);
    if (!entries.is_ok()) {
        return -ENOENT;
    }

    struct fuse_stat st {};
    const auto file_mode = state->options.read_only ? 0444 : 0644;
    const auto dir_mode = state->options.read_only ? 0555 : 0755;
    st.st_mode = S_IFDIR | dir_mode;
    filler(buf, ".", &st, 0, static_cast<fuse_fill_dir_flags>(0));
    filler(buf, "..", &st, 0, static_cast<fuse_fill_dir_flags>(0));

    for (const auto& entry : entries.value()) {
        std::memset(&st, 0, sizeof(st));
        st.st_mode = entry.type == "dir" ? (S_IFDIR | dir_mode) : (S_IFREG | file_mode);
        st.st_size = static_cast<fuse_off_t>(entry.size);
        st.st_mtim.tv_sec = static_cast<decltype(st.st_mtim.tv_sec)>(entry.mtime);
        filler(buf, entry.name.c_str(), &st, 0, static_cast<fuse_fill_dir_flags>(0));
    }
    return 0;
}

int svfs_mkdir(const char* path, fuse_mode_t mode) {
    (void)mode;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->mkdir(path));
}

int svfs_unlink(const char* path) {
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->unlink(path));
}

int svfs_rmdir(const char* path) {
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->rmdir(path));
}

int svfs_rename(const char* oldpath, const char* newpath, unsigned int flags) {
    (void)flags;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->rename(oldpath, newpath));
}

int svfs_truncate(const char* path, fuse_off_t size, struct fuse_file_info* fi) {
    (void)fi;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->truncate(path, static_cast<std::uint64_t>(size)));
}

int svfs_open(const char* path, struct fuse_file_info* fi) {
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }

    if ((fi->flags & O_CREAT) != 0) {
        const auto attr = state->svfs->getattr(path);
        if (!attr.is_ok()) {
            const auto created = state->svfs->create_file(path);
            if (!created.is_ok()) {
                return result_to_errno(created);
            }
        }
    }

    const bool write_requested = (fi->flags & (O_WRONLY | O_RDWR | O_TRUNC)) != 0;
    auto handle = state->svfs->open(path, write_requested);
    if (!handle.is_ok() && write_requested) {
        const auto created = state->svfs->create_file(path);
        if (created.is_ok()) {
            handle = state->svfs->open(path, write_requested);
        }
    }
    if (!handle.is_ok()) {
        return result_to_errno(Result<void>::err(handle.error(), handle.message()));
    }
    fi->fh = static_cast<std::uint64_t>(handle.value());
    return 0;
}

int svfs_read(
    const char* path,
    char* buf,
    size_t size,
    fuse_off_t off,
    struct fuse_file_info* fi) {
    (void)path;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }

    const auto read = state->svfs->read(
        static_cast<int>(fi->fh),
        static_cast<std::uint64_t>(off),
        Span<std::uint8_t>(reinterpret_cast<std::uint8_t*>(buf), size));
    if (!read.is_ok()) {
        return result_to_errno(Result<void>::err(read.error(), read.message()));
    }
    return static_cast<int>(read.value());
}

int svfs_write(
    const char* path,
    const char* buf,
    size_t size,
    fuse_off_t off,
    struct fuse_file_info* fi) {
    (void)path;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }

    const auto written = state->svfs->write(
        static_cast<int>(fi->fh),
        static_cast<std::uint64_t>(off),
        Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(buf), size));
    if (!written.is_ok()) {
        return result_to_errno(Result<void>::err(written.error(), written.message()));
    }
    return static_cast<int>(written.value());
}

int svfs_release(const char* path, struct fuse_file_info* fi) {
    (void)path;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->release(static_cast<int>(fi->fh)));
}

int svfs_create(const char* path, fuse_mode_t mode, struct fuse_file_info* fi) {
    (void)mode;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }

    const auto created = state->svfs->create_file(path);
    if (!created.is_ok()) {
        return result_to_errno(created);
    }
    return svfs_open(path, fi);
}

int svfs_flush(const char* path, struct fuse_file_info* fi) {
    (void)path;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->flush(static_cast<int>(fi->fh)));
}

int svfs_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
    (void)path;
    (void)datasync;
    auto* state = state_from_context();
    if (state == nullptr || state->svfs == nullptr) {
        return -EIO;
    }
    return result_to_errno(state->svfs->flush(static_cast<int>(fi->fh)));
}

fuse_operations make_operations() {
    fuse_operations ops {};
    ops.getattr = svfs_getattr;
    ops.readdir = svfs_readdir;
    ops.mkdir = svfs_mkdir;
    ops.unlink = svfs_unlink;
    ops.rmdir = svfs_rmdir;
    ops.rename = svfs_rename;
    ops.truncate = svfs_truncate;
    ops.open = svfs_open;
    ops.read = svfs_read;
    ops.write = svfs_write;
    ops.release = svfs_release;
    ops.create = svfs_create;
    ops.flush = svfs_flush;
    ops.fsync = svfs_fsync;
    return ops;
}

std::string mount_key(const std::filesystem::path& mount_point) {
    return mount_point.generic_string();
}

#endif

} 

FuseMountBackend::~FuseMountBackend() = default;

FuseMountBackend::FuseMountBackend() = default;

std::string FuseMountBackend::name() const {
    return "fuse";
}

Result<void> FuseMountBackend::mount(
    const std::filesystem::path& vault_path,
    const MountOptions& opts) {
#if !defined(SV_HAVE_WINFSP) && !defined(SV_HAVE_FUSE3)
    (void)vault_path;
    (void)opts;
    return Result<void>::err(ErrorCode::NotImplemented, "FUSE mount backend not available on this platform");
#else
    const auto session = vault::SessionStore::instance().find(vault_path);
    if (!session.is_ok()) {
        return Result<void>::err(ErrorCode::VaultLocked, "unlock vault before mount");
    }

    const auto key = mount_key(opts.mount_point);
    {
        std::lock_guard lock(g_fuse_mutex);
        if (g_fuse_mounts.contains(key)) {
            return Result<void>::err(ErrorCode::InvalidArgument, "mount point already in use");
        }
    }

    auto state = std::make_unique<FuseMountState>();
    state->mount_point = opts.mount_point;
    state->vault_path = vault_path;
    state->options = opts;
    state->engine = std::make_unique<vault::VaultEngine>();

    const auto touch = [key]() {
        std::lock_guard lock(g_fuse_mutex);
        const auto it = g_fuse_mounts.find(key);
        if (it != g_fuse_mounts.end()) {
            
            (void)it;
        }
    };
    state->svfs = std::make_unique<SvFs>(*state->engine, *session.value(), opts, touch);

    struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);
    fuse_opt_add_arg(&args, "shadowvault");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "FileSystemName=ShadowVault");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "volname=ShadowVault");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "uid=-1");
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "gid=-1");
    if (opts.read_only) {
        fuse_opt_add_arg(&args, "-oro");
    }

    const auto mount_point_utf8 = opts.mount_point.generic_string();
    const auto ops = make_operations();
    state->fuse_handle = fuse_new(&args, &ops, sizeof(ops), state.get());
    fuse_opt_free_args(&args);
    if (state->fuse_handle == nullptr) {
        return Result<void>::err(ErrorCode::PlatformError, "fuse_new failed");
    }

    if (fuse_mount(state->fuse_handle, mount_point_utf8.c_str()) != 0) {
        fuse_destroy(state->fuse_handle);
        return Result<void>::err(ErrorCode::PlatformError, "fuse_mount failed");
    }

    state->running.store(true);
    state->fuse_thread = std::thread([state_ptr = state.get()]() {
        fuse_loop(state_ptr->fuse_handle);
        state_ptr->running.store(false);
    });

    {
        std::lock_guard lock(g_fuse_mutex);
        g_fuse_mounts.emplace(key, std::move(state));
    }

    return Result<void>::ok();
#endif
}

Result<void> FuseMountBackend::dismount(const std::filesystem::path& mount_point) {
#if !defined(SV_HAVE_WINFSP) && !defined(SV_HAVE_FUSE3)
    (void)mount_point;
    return Result<void>::err(ErrorCode::NotImplemented, "FUSE mount backend not available on this platform");
#else
    const auto key = mount_key(mount_point);
    std::unique_ptr<FuseMountState> state;
    {
        std::lock_guard lock(g_fuse_mutex);
        const auto it = g_fuse_mounts.find(key);
        if (it == g_fuse_mounts.end()) {
            return Result<void>::err(ErrorCode::InvalidArgument, "No session for mount point");
        }
        state = std::move(it->second);
        g_fuse_mounts.erase(it);
    }

    if (state->fuse_handle != nullptr) {
        fuse_exit(state->fuse_handle);
        if (state->fuse_thread.joinable()) {
            state->fuse_thread.join();
        }

        
        state->svfs.reset();
        state->engine.reset();

#if defined(SV_HAVE_WINFSP)
        
        
        fuse_destroy(state->fuse_handle);
#else
        fuse_unmount(state->fuse_handle);
        fuse_destroy(state->fuse_handle);
#endif
        state->fuse_handle = nullptr;
    }
    return Result<void>::ok();
#endif
}

std::vector<MountSession> FuseMountBackend::list_active() const {
#if !defined(SV_HAVE_WINFSP) && !defined(SV_HAVE_FUSE3)
    return {};
#else
    std::lock_guard lock(g_fuse_mutex);
    std::vector<MountSession> sessions;
    sessions.reserve(g_fuse_mounts.size());
    for (const auto& [key, state] : g_fuse_mounts) {
        (void)key;
        MountSession session {};
        session.mount_point = state->mount_point;
        session.vault_path = state->vault_path;
        session.read_only = state->options.read_only;
        session.auto_dismount = state->options.auto_dismount;
        session.idle_timeout = state->options.idle_timeout;
        sessions.push_back(session);
    }
    return sessions;
#endif
}

} 
