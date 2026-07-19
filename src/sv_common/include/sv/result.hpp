#pragma once

#include <sv/exit_codes.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace sv {

enum class ErrorCode : std::uint32_t {
    Ok = 0,
    InvalidArgument,
    IoError,
    CorruptContainer,
    WrongCredentials,
    HeaderDecryptFailed,
    ManifestDecryptFailed,
    HiddenVolumeProtected,
    TransactionIncomplete,
    ManifestFull,
    UnsupportedVersion,
    PlatformError,
    NotImplemented,
    VaultLocked,
    UsageError,
};


constexpr int exit_code_for(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Ok:
        return exit_codes::success;
    case ErrorCode::UsageError:
    case ErrorCode::InvalidArgument:
        return exit_codes::usage_error;
    case ErrorCode::WrongCredentials:
    case ErrorCode::HeaderDecryptFailed:
        return exit_codes::wrong_credentials;
    case ErrorCode::CorruptContainer:
    case ErrorCode::ManifestDecryptFailed:
        return exit_codes::corrupt_container;
    case ErrorCode::HiddenVolumeProtected:
        return exit_codes::hidden_volume_protected;
    case ErrorCode::VaultLocked:
        return exit_codes::vault_locked;
    case ErrorCode::IoError:
        return exit_codes::io_error;
    case ErrorCode::ManifestFull:
        return exit_codes::manifest_full;
    case ErrorCode::TransactionIncomplete:
    case ErrorCode::UnsupportedVersion:
    case ErrorCode::PlatformError:
    case ErrorCode::NotImplemented:
    default:
        return exit_codes::general_error;
    }
}

const char* error_code_name(ErrorCode code);

struct Error {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
};

namespace detail {

template<typename T>
class ResultStorage {
public:
    static ResultStorage ok(T value) {
        ResultStorage result;
        result.data_ = std::move(value);
        return result;
    }

    static ResultStorage err(ErrorCode code, std::string message = {}) {
        ResultStorage result;
        result.data_ = Error{code, std::move(message)};
        return result;
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] T& value() & {
        if (!is_ok()) {
            throw std::logic_error("sv::Result::value() called on error result");
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const& {
        if (!is_ok()) {
            throw std::logic_error("sv::Result::value() called on error result");
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        if (!is_ok()) {
            throw std::logic_error("sv::Result::value() called on error result");
        }
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] ErrorCode error() const noexcept {
        if (is_ok()) {
            return ErrorCode::Ok;
        }
        return std::get<Error>(data_).code;
    }

    [[nodiscard]] const std::string& message() const noexcept {
        static const std::string empty;
        if (is_ok()) {
            return empty;
        }
        return std::get<Error>(data_).message;
    }

private:
    std::variant<T, Error> data_{T{}};
};

template<>
class ResultStorage<void> {
public:
    static ResultStorage ok() {
        ResultStorage result;
        result.ok_ = true;
        return result;
    }

    static ResultStorage err(ErrorCode code, std::string message = {}) {
        ResultStorage result;
        result.error_ = Error{code, std::move(message)};
        return result;
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return ok_;
    }

    void value() const {
        if (!is_ok()) {
            throw std::logic_error("sv::Result::value() called on error result");
        }
    }

    [[nodiscard]] ErrorCode error() const noexcept {
        if (is_ok()) {
            return ErrorCode::Ok;
        }
        return error_->code;
    }

    [[nodiscard]] const std::string& message() const noexcept {
        static const std::string empty;
        if (is_ok()) {
            return empty;
        }
        return error_->message;
    }

private:
    bool ok_ = false;
    std::optional<Error> error_;
};

} 

template<typename T>
class Result {
public:
    static Result ok(T value) {
        Result result;
        result.storage_ = detail::ResultStorage<T>::ok(std::move(value));
        return result;
    }

    static Result err(ErrorCode code, std::string message = {}) {
        Result result;
        result.storage_ = detail::ResultStorage<T>::err(code, std::move(message));
        return result;
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return storage_.is_ok();
    }

    [[nodiscard]] T& value() & {
        return storage_.value();
    }

    [[nodiscard]] const T& value() const& {
        return storage_.value();
    }

    [[nodiscard]] T&& value() && {
        return std::move(storage_).value();
    }

    [[nodiscard]] ErrorCode error() const noexcept {
        return storage_.error();
    }

    [[nodiscard]] const std::string& message() const noexcept {
        return storage_.message();
    }

private:
    detail::ResultStorage<T> storage_{};
};

template<>
class Result<void> {
public:
    static Result ok() {
        Result result;
        result.storage_ = detail::ResultStorage<void>::ok();
        return result;
    }

    static Result err(ErrorCode code, std::string message = {}) {
        Result result;
        result.storage_ = detail::ResultStorage<void>::err(code, std::move(message));
        return result;
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return storage_.is_ok();
    }

    void value() const {
        storage_.value();
    }

    [[nodiscard]] ErrorCode error() const noexcept {
        return storage_.error();
    }

    [[nodiscard]] const std::string& message() const noexcept {
        return storage_.message();
    }

private:
    detail::ResultStorage<void> storage_{};
};

} 