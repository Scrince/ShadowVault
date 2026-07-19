#include <sv/platform/terminal_secret.hpp>

#include "terminal_secret_detail.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstring>
#endif

#include <iostream>
#include <utility>

namespace sv::platform {

namespace {

constexpr auto kSecureWipePattern = static_cast<unsigned char>(0xFF);

} 

void secure_wipe(std::string& value) noexcept {
    if (!value.empty()) {
#if defined(_WIN32)
        SecureZeroMemory(value.data(), value.size());
#else
#if defined(__STDC_LIB_EXT1__)
        std::memset_s(value.data(), value.size(), 0, value.size());
#else
        volatile unsigned char* ptr = reinterpret_cast<volatile unsigned char*>(value.data());
        for (std::size_t i = 0; i < value.size(); ++i) {
            ptr[i] = 0;
            ptr[i] = kSecureWipePattern;
            ptr[i] = 0;
        }
#endif
#endif
    }
    value.clear();
    value.shrink_to_fit();
}

SecretString::SecretString(std::string value)
    : data_(std::move(value)) {}

SecretString::~SecretString() {
    wipe();
}

SecretString::SecretString(SecretString&& other) noexcept
    : data_(std::move(other.data_)) {
    other.data_.clear();
}

SecretString& SecretString::operator=(SecretString&& other) noexcept {
    if (this != &other) {
        wipe();
        data_ = std::move(other.data_);
        other.data_.clear();
    }
    return *this;
}

std::string_view SecretString::view() const noexcept {
    return data_;
}

bool SecretString::empty() const noexcept {
    return data_.empty();
}

void SecretString::clear() {
    wipe();
}

std::string SecretString::release() {
    std::string released = std::move(data_);
    data_.clear();
    return released;
}

void SecretString::wipe() noexcept {
    secure_wipe(data_);
}

Result<SecretString> read_secret(const std::string_view prompt, const bool confirm) {
    if (prompt.empty()) {
        return Result<SecretString>::err(ErrorCode::InvalidArgument, "prompt must not be empty");
    }

    std::cout << prompt << std::flush;
    auto first = detail::read_line_no_echo();
    std::cout << '\n';
    if (!first.is_ok()) {
        return Result<SecretString>::err(first.error(), first.message());
    }

    if (!confirm) {
        return Result<SecretString>::ok(SecretString(std::move(first.value())));
    }

    std::cout << "Confirm: " << std::flush;
    auto second = detail::read_line_no_echo();
    std::cout << '\n';
    if (!second.is_ok()) {
        secure_wipe(first.value());
        return Result<SecretString>::err(second.error(), second.message());
    }

    if (first.value() != second.value()) {
        secure_wipe(first.value());
        secure_wipe(second.value());
        return Result<SecretString>::err(ErrorCode::InvalidArgument, "secrets do not match");
    }

    secure_wipe(second.value());
    return Result<SecretString>::ok(SecretString(std::move(first.value())));
}

} 