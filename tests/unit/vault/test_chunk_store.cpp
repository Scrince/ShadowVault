#include <sv/container/container_file.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/vault/chunk_store.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>

namespace {

std::filesystem::path temp_container_path() {
    static std::atomic<std::uint64_t> counter {0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path()
        / ("shadowvault_chunk_store_"
           + std::to_string(stamp)
           + "_"
           + std::to_string(counter.fetch_add(1, std::memory_order_relaxed))
           + ".bin");
}

} // namespace

TEST_CASE("chunk AEAD binds full manifest generation", "[unit][vault][chunk]") {
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const auto path = temp_container_path();
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup {path};

    auto container = sv::container::ContainerFileStore::create(path, 4096);
    REQUIRE(container.is_ok());

    constexpr std::uint32_t kChunkSize = 64;
    sv::vault::ChunkStore chunks(container.value(), *crypto, 0, kChunkSize);

    sv::vault::Inode inode {};
    inode.inode_id = 42;
    inode.size = 11;
    inode.base_nonce.fill(0xA5);

    std::array<std::uint8_t, sv::crypto::kAeadKeySize> vmk {};
    vmk.fill(0x5C);

    const std::array<std::uint8_t, 11> plaintext {
        'h', 'e', 'l', 'l', 'o', '-', 'c', 'h', 'u', 'n', 'k',
    };
    constexpr std::uint64_t kGeneration = 0x0000000200000001ULL;

    const auto written = chunks.write_file(inode, vmk, kGeneration, plaintext, 0);
    REQUIRE(written.is_ok());
    inode.chunk_count = written.value().chunk_count;
    inode.first_chunk_id = written.value().first_chunk_id;

    const auto read_ok = chunks.read_file(inode, vmk, kGeneration);
    REQUIRE(read_ok.is_ok());
    REQUIRE(read_ok.value() == std::vector<std::uint8_t>(plaintext.begin(), plaintext.end()));

    const auto read_wrong_high_generation = chunks.read_file(inode, vmk, 0x0000000300000001ULL);
    REQUIRE_FALSE(read_wrong_high_generation.is_ok());
}

TEST_CASE("chunk store rejects zero chunk size", "[unit][vault][chunk]") {
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const auto path = temp_container_path();
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup {path};

    auto container = sv::container::ContainerFileStore::create(path, 4096);
    REQUIRE(container.is_ok());

    sv::vault::ChunkStore chunks(container.value(), *crypto, 0, 0);

    sv::vault::Inode inode {};
    inode.inode_id = 42;
    inode.size = 1;
    inode.chunk_count = 1;
    inode.base_nonce.fill(0xA5);

    std::array<std::uint8_t, sv::crypto::kAeadKeySize> vmk {};
    vmk.fill(0x5C);
    const std::array<std::uint8_t, 1> plaintext {'x'};

    const auto written = chunks.write_file(inode, vmk, 1, plaintext, 0);
    REQUIRE_FALSE(written.is_ok());
    REQUIRE(written.error() == sv::ErrorCode::CorruptContainer);

    const auto read = chunks.read_file(inode, vmk, 1);
    REQUIRE_FALSE(read.is_ok());
    REQUIRE(read.error() == sv::ErrorCode::CorruptContainer);
}
