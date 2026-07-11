#include <sv/crypto/secure_buffer.hpp>

#include "secure_memory_detail.hpp"

#include <utility>

namespace sv::crypto {

SecureBuffer::SecureBuffer(std::size_t size) {
    resize(size);
}

SecureBuffer::~SecureBuffer() {
    release();
}

SecureBuffer::SecureBuffer(SecureBuffer&& other) noexcept
    : bytes_(std::move(other.bytes_))
    , locked_(other.locked_) {
    other.locked_ = false;
}

SecureBuffer& SecureBuffer::operator=(SecureBuffer&& other) noexcept {
    if (this != &other) {
        release();
        bytes_ = std::move(other.bytes_);
        locked_ = other.locked_;
        other.locked_ = false;
    }
    return *this;
}

void SecureBuffer::resize(std::size_t new_size) {
    if (new_size == bytes_.size()) {
        return;
    }

    if (locked_ && !bytes_.empty()) {
        (void)detail::unlock_secure_region(
            Span<std::byte>(reinterpret_cast<std::byte*>(bytes_.data()), bytes_.size()));
        locked_ = false;
    }

    if (new_size < bytes_.size()) {
        detail::wipe_secure_region(Span<std::uint8_t>(bytes_.data() + new_size, bytes_.size() - new_size));
    }

    bytes_.resize(new_size);

    if (!bytes_.empty()) {
        const auto lock_result = detail::lock_secure_region(
            Span<std::byte>(reinterpret_cast<std::byte*>(bytes_.data()), bytes_.size()));
        locked_ = lock_result.is_ok();
    }
}

void SecureBuffer::clear() noexcept {
    release();
}

void SecureBuffer::release() noexcept {
    if (!bytes_.empty()) {
        detail::wipe_secure_region(span());
        if (locked_) {
            (void)detail::unlock_secure_region(
                Span<std::byte>(reinterpret_cast<std::byte*>(bytes_.data()), bytes_.size()));
        }
    }
    bytes_.clear();
    locked_ = false;
}

} // namespace sv::crypto