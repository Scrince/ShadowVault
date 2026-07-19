#pragma once

#include <sv/crypto/crypto_provider.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace sv::crypto {


class SecureBuffer {
public:
    SecureBuffer() = default;
    explicit SecureBuffer(std::size_t size);
    ~SecureBuffer();

    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    SecureBuffer(SecureBuffer&& other) noexcept;
    SecureBuffer& operator=(SecureBuffer&& other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }

    [[nodiscard]] std::uint8_t* data() noexcept { return bytes_.data(); }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return bytes_.data(); }

    [[nodiscard]] Span<std::uint8_t> span() noexcept { return Span<std::uint8_t>(bytes_.data(), bytes_.size()); }
    [[nodiscard]] Span<const std::uint8_t> span() const noexcept {
        return Span<const std::uint8_t>(bytes_.data(), bytes_.size());
    }

    void resize(std::size_t new_size);
    void clear() noexcept;

private:
    void release() noexcept;

    std::vector<std::uint8_t> bytes_;
    bool locked_ = false;
};

} 