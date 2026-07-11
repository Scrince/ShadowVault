#include <sv/mount/mount_manager.hpp>

#include <sv/mount/backends/fuse_backend.hpp>
#include <sv/mount/backends/stub_backend.hpp>

#include <atomic>
#include <cstdlib>

namespace sv::mount {

namespace {

// WinFsp FUSE teardown can leave MountManager::mutex_ unsafe to lock during
// std::atexit (after all tests have already dismounted). Track active mounts
// atomically so the atexit handler can skip cleanup when count is zero.
std::atomic<std::size_t> g_active_mount_sessions{0};

std::unique_ptr<MountBackend> create_default_backend() {
#if defined(SV_HAVE_WINFSP) || defined(SV_HAVE_FUSE3)
    return std::make_unique<FuseMountBackend>();
#else
    return std::make_unique<StubBackend>();
#endif
}

void ensure_atexit_registered() {
    static bool registered = false;
    if (!registered) {
        std::atexit(+[]() {
            if (g_active_mount_sessions.load(std::memory_order_acquire) == 0) {
                return;
            }
            (void)MountManager::instance().dismount_all();
            g_active_mount_sessions.store(0, std::memory_order_release);
        });
        registered = true;
    }
}

} // namespace

MountManager& MountManager::instance() {
    static MountManager manager;
    return manager;
}

MountManager::MountManager()
    : backend_(create_default_backend()) {
    ensure_atexit_registered();
}

Result<MountSessionId> MountManager::mount_vault(
    const std::filesystem::path& vault_path,
    MountOptions opts) {
    if (vault_path.empty()) {
        return Result<MountSessionId>::err(ErrorCode::InvalidArgument, "Vault path is required");
    }
    if (opts.mount_point.empty()) {
        return Result<MountSessionId>::err(ErrorCode::InvalidArgument, "Mount point is required");
    }

    std::lock_guard lock(mutex_);

    for (const auto& [id, session] : sessions_) {
        (void)id;
        if (session.mount_point == opts.mount_point) {
            return Result<MountSessionId>::err(
                ErrorCode::InvalidArgument,
                "Mount point already in use");
        }
    }

    const auto backend_result = backend_->mount(vault_path, opts);
    if (!backend_result.is_ok()) {
        return Result<MountSessionId>::err(backend_result.error(), backend_result.message());
    }

    const auto session_id = next_session_id();
    MountSession session {
        .session_id = session_id,
        .mount_point = opts.mount_point,
        .vault_path = vault_path,
        .read_only = opts.read_only,
        .auto_dismount = opts.auto_dismount,
        .idle_timeout = opts.idle_timeout,
        .last_io_time = std::chrono::system_clock::now(),
    };

    sessions_.emplace(session_id, session);
    g_active_mount_sessions.fetch_add(1, std::memory_order_release);
    return Result<MountSessionId>::ok(session_id);
}

Result<void> MountManager::dismount(MountSessionId id) {
    std::lock_guard lock(mutex_);

    const auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return Result<void>::err(ErrorCode::InvalidArgument, "Unknown mount session");
    }

    const auto backend_result = backend_->dismount(it->second.mount_point);
    if (!backend_result.is_ok()) {
        return backend_result;
    }

    sessions_.erase(it);
    g_active_mount_sessions.fetch_sub(1, std::memory_order_release);
    return Result<void>::ok();
}

Result<void> MountManager::dismount_point(const std::filesystem::path& mount_point) {
    MountSessionId target_id = 0;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [id, session] : sessions_) {
            if (session.mount_point == mount_point) {
                target_id = id;
                break;
            }
        }
    }

    if (target_id == 0) {
        return Result<void>::err(ErrorCode::InvalidArgument, "No session for mount point");
    }

    return dismount(target_id);
}

Result<void> MountManager::dismount_all() {
    std::vector<MountSessionId> ids;
    {
        std::lock_guard lock(mutex_);
        ids.reserve(sessions_.size());
        for (const auto& [id, session] : sessions_) {
            (void)session;
            ids.push_back(id);
        }
    }

    for (const auto id : ids) {
        const auto result = dismount(id);
        if (!result.is_ok()) {
            return result;
        }
    }

    return Result<void>::ok();
}

std::vector<MountSession> MountManager::list_sessions() const {
    std::lock_guard lock(mutex_);
    std::vector<MountSession> out;
    out.reserve(sessions_.size());
    for (const auto& [id, session] : sessions_) {
        (void)id;
        out.push_back(session);
    }
    return out;
}

void MountManager::touch_io(MountSessionId id) {
    std::lock_guard lock(mutex_);
    const auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        it->second.last_io_time = std::chrono::system_clock::now();
    }
}

void MountManager::on_idle_tick() {
    const auto now = std::chrono::system_clock::now();
    std::vector<MountSessionId> expired;

    {
        std::lock_guard lock(mutex_);
        for (const auto& [id, session] : sessions_) {
            if (!session.auto_dismount) {
                continue;
            }
            const auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - session.last_io_time);
            if (idle >= session.idle_timeout) {
                expired.push_back(id);
            }
        }
    }

    for (const auto id : expired) {
        (void)dismount(id);
    }
}

MountSessionId MountManager::next_session_id() {
    return next_id_++;
}

} // namespace sv::mount