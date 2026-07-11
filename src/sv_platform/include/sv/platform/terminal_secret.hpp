#pragma once

#include <sv/result.hpp>

#include <string>
#include <string_view>

namespace sv::platform {

/// Owns secret bytes and scrubs the backing buffer on destruction.
class SecretString {
public:
    SecretString() = default;
    explicit SecretString(std::string value);
    ~SecretString();

    SecretString(SecretString&& other) noexcept;
    SecretString& operator=(SecretString&& other) noexcept;

    SecretString(const SecretString&) = delete;
    SecretString& operator=(const SecretString&) = delete;

    [[nodiscard]] std::string_view view() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear();

    /// Transfers ownership; caller must wipe the returned buffer when done.
    [[nodiscard]] std::string release();

private:
    void wipe() noexcept;

    std::string data_;
};

/// Read a secret from the terminal with echo disabled.
/// When confirm is true, prompts twice and requires a match.
[[nodiscard]] Result<SecretString> read_secret(std::string_view prompt, bool confirm = false);

void secure_wipe(std::string& value) noexcept;

} // namespace sv::platform