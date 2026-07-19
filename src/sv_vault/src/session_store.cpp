#include <sv/vault/session_store.hpp>

namespace sv::vault {

SessionStore& SessionStore::instance() noexcept {
    static SessionStore store;
    return store;
}

bool SessionStore::has_session(const std::filesystem::path& path) const noexcept {
    const auto absolute = std::filesystem::weakly_canonical(path);
    for (const auto& session : sessions_) {
        if (std::filesystem::weakly_canonical(session->path) == absolute) {
            return true;
        }
    }
    return false;
}

Result<UnlockedVault*> SessionStore::find(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::weakly_canonical(path);
    for (const auto& session : sessions_) {
        if (std::filesystem::weakly_canonical(session->path) == absolute) {
            return Result<UnlockedVault*>::ok(session.get());
        }
    }
    return Result<UnlockedVault*>::err(ErrorCode::VaultLocked, "no active session for vault");
}

Result<UnlockedVault*> SessionStore::find_any() {
    if (sessions_.empty()) {
        return Result<UnlockedVault*>::err(ErrorCode::VaultLocked, "no active vault session");
    }
    return Result<UnlockedVault*>::ok(sessions_.front().get());
}

Result<void> SessionStore::insert(UnlockedVault session, const bool replace) {
    const auto absolute = std::filesystem::weakly_canonical(session.path);
    session.path = absolute;

    for (std::size_t i = 0; i < sessions_.size(); ++i) {
        if (std::filesystem::weakly_canonical(sessions_[i]->path) == absolute) {
            if (!replace) {
                return Result<void>::err(ErrorCode::InvalidArgument, "vault session already active");
            }
            sessions_[i] = std::make_unique<UnlockedVault>(std::move(session));
            return Result<void>::ok();
        }
    }

    sessions_.push_back(std::make_unique<UnlockedVault>(std::move(session)));
    return Result<void>::ok();
}

Result<void> SessionStore::remove(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::weakly_canonical(path);
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (std::filesystem::weakly_canonical((*it)->path) == absolute) {
            sessions_.erase(it);
            return Result<void>::ok();
        }
    }
    return Result<void>::err(ErrorCode::VaultLocked, "no active session for vault");
}

void SessionStore::clear() noexcept {
    sessions_.clear();
}

} 