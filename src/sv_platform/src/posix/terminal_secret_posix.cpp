#if !defined(_WIN32)

#include "../terminal_secret_detail.hpp"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace sv::platform::detail {

namespace {

class EchoGuard {
public:
    explicit EchoGuard(const int fd)
        : fd_(fd) {
        if (tcgetattr(fd_, &original_) != 0) {
            valid_ = false;
            return;
        }

        termios no_echo = original_;
        no_echo.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        if (tcsetattr(fd_, TCSANOW, &no_echo) != 0) {
            valid_ = false;
        }
    }

    ~EchoGuard() {
        if (valid_) {
            tcsetattr(fd_, TCSANOW, &original_);
        }
    }

    [[nodiscard]] bool valid() const noexcept { return valid_; }

    EchoGuard(const EchoGuard&) = delete;
    EchoGuard& operator=(const EchoGuard&) = delete;

private:
    int fd_{-1};
    termios original_{};
    bool valid_{true};
};

} 

Result<std::string> read_line_no_echo() {
    if (!isatty(STDIN_FILENO)) {
        return Result<std::string>::err(
            ErrorCode::PlatformError,
            "stdin is not a terminal");
    }

    EchoGuard guard(STDIN_FILENO);
    if (!guard.valid()) {
        return Result<std::string>::err(
            ErrorCode::PlatformError,
            std::string("failed to configure terminal: ") + std::strerror(errno));
    }

    std::string value;
    char ch = '\0';
    while (true) {
        const ssize_t read = ::read(STDIN_FILENO, &ch, 1);
        if (read < 0) {
            return Result<std::string>::err(
                ErrorCode::PlatformError,
                std::string("read failed: ") + std::strerror(errno));
        }
        if (read == 0) {
            break;
        }

        if (ch == '\n' || ch == '\r') {
            break;
        }
        if (ch == 127 || ch == '\b') {
            if (!value.empty()) {
                value.pop_back();
            }
            continue;
        }
        if (ch == 3) {
            return Result<std::string>::err(ErrorCode::PlatformError, "input interrupted");
        }
        if (std::isprint(static_cast<unsigned char>(ch)) != 0) {
            value.push_back(ch);
        }
    }

    return Result<std::string>::ok(std::move(value));
}

} 

#endif