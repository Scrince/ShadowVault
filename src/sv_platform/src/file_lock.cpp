#include <sv/platform/file_lock.hpp>

#include "file_lock_detail.hpp"

namespace sv::platform {

struct FileLock::Impl {
    std::unique_ptr<detail::FileLockState> state;
};

FileLock::FileLock(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

FileLock::~FileLock() = default;

FileLock::FileLock(FileLock&& other) noexcept = default;

FileLock& FileLock::operator=(FileLock&& other) noexcept = default;

Result<FileLock> FileLock::acquire(const std::filesystem::path& path) {
    if (path.empty()) {
        return Result<FileLock>::err(ErrorCode::InvalidArgument, "path must not be empty");
    }

    auto state = detail::open_exclusive_lock(path);
    if (!state.is_ok()) {
        return Result<FileLock>::err(state.error(), state.message());
    }

    auto impl = std::make_unique<Impl>();
    impl->state = std::move(state.value());
    return Result<FileLock>::ok(FileLock(std::move(impl)));
}

bool FileLock::is_locked() const noexcept {
    return static_cast<bool>(impl_ && impl_->state);
}

const std::filesystem::path& FileLock::path() const noexcept {
    static const std::filesystem::path kEmpty;
    if (!impl_ || !impl_->state) {
        return kEmpty;
    }
    return impl_->state->path;
}

} // namespace sv::platform