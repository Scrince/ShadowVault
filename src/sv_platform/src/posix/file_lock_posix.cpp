#if !defined(_WIN32)

#include "../file_lock_detail.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <unistd.h>

namespace sv::platform::detail {

FileLockState::~FileLockState() {
    if (fd >= 0) {
        ::flock(fd, LOCK_UN);
        ::close(fd);
        fd = -1;
    }
}

Result<std::unique_ptr<FileLockState>> open_exclusive_lock(
    const std::filesystem::path& path) {
    const auto native = path.native();
    const int fd = ::open(native.c_str(), O_RDWR);
    if (fd < 0) {
        return Result<std::unique_ptr<FileLockState>>::err(
            ErrorCode::IoError,
            std::string("open failed: ") + std::strerror(errno));
    }

    if (::flock(fd, LOCK_EX) != 0) {
        const int error = errno;
        ::close(fd);
        return Result<std::unique_ptr<FileLockState>>::err(
            ErrorCode::IoError,
            std::string("flock failed: ") + std::strerror(error));
    }

    auto state = std::make_unique<FileLockState>();
    state->path = path;
    state->fd = fd;
    return Result<std::unique_ptr<FileLockState>>::ok(std::move(state));
}

} 

#endif