#include "test_fixtures.hpp"

#include <sv/container/container_file.hpp>
#include <sv/pba/pba_unlock.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <vector>

namespace {

std::array<std::uint8_t, 32> vmk_from_session(const sv::vault::UnlockedVault& session) {
    std::array<std::uint8_t, 32> vmk {};
    REQUIRE(session.vmk.size() == vmk.size());
    std::memcpy(vmk.data(), session.vmk.data(), vmk.size());
    return vmk;
}

void fill_unlock_request(
    sv::container::ContainerFileStore& container,
    sv::Span<const std::uint8_t> password_utf8,
    std::vector<std::uint8_t>& header_storage,
    std::vector<std::uint8_t>& superblock_storage,
    sv::pba::PbaUnlockRequest& request) {
    const auto tail_sb = container.read_superblock_tail();
    REQUIRE(tail_sb.is_ok());

    const auto header_bytes = container.read_region(
        tail_sb.value().primary_header_offset,
        sv::container::SVLT_HEADER_SIZE);
    REQUIRE(header_bytes.is_ok());
    header_storage = std::move(header_bytes.value());

    const auto superblock_offset = tail_sb.value().container_size - sv::container::SVLT_SUPERBLOCK_SIZE;
    const auto superblock_bytes = container.read_region(
        superblock_offset,
        sv::container::SVLT_SUPERBLOCK_SIZE);
    REQUIRE(superblock_bytes.is_ok());
    superblock_storage = std::move(superblock_bytes.value());

    request.primary_header_sector = sv::Span<const std::uint8_t>(
        header_storage.data(),
        header_storage.size());
    request.superblock_bytes = sv::Span<const std::uint8_t>(
        superblock_storage.data(),
        superblock_storage.size());
    request.credentials.password_utf8 = password_utf8;
}

} 

TEST_CASE("P1-04 unlock_primary_header on real .svlt vault", "[integration][pba][p104]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto path = sv::test::temp_vault_path("sv_test_pba_vault");
    sv::vault::VaultEngine engine;
    const auto cred = sv::test::outer_credentials();
    const auto opts = sv::test::file_ops_create_options();

    REQUIRE(engine.create_new(path, cred, opts).is_ok());

    sv::vault::OpenVaultOptions open_opts {};
    auto session = engine.open(path, cred, open_opts);
    REQUIRE(session.is_ok());
    const auto expected_vmk = vmk_from_session(session.value());
    sv::test::release_session(session.value());

    auto container = sv::container::ContainerFileStore::open(path);
    REQUIRE(container.is_ok());

    std::vector<std::uint8_t> header_storage;
    std::vector<std::uint8_t> superblock_storage;
    sv::pba::PbaUnlockRequest request {};
    fill_unlock_request(
        container.value(),
        sv::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(cred.password_utf8.data()),
            cred.password_utf8.size()),
        header_storage,
        superblock_storage,
        request);

    const auto unlocked = sv::pba::unlock_primary_header(request);
    REQUIRE(unlocked.is_ok());
    REQUIRE(unlocked.value() == expected_vmk);

    sv::test::cleanup_vault(path);
}

TEST_CASE("P1-05 wrong password on real .svlt vault", "[integration][pba][p105]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto path = sv::test::temp_vault_path("sv_test_pba_wrong");
    sv::vault::VaultEngine engine;
    const auto cred = sv::test::outer_credentials();
    const auto opts = sv::test::file_ops_create_options();

    REQUIRE(engine.create_new(path, cred, opts).is_ok());

    auto container = sv::container::ContainerFileStore::open(path);
    REQUIRE(container.is_ok());

    const char* wrong_password = "not-the-integration-password";
    std::vector<std::uint8_t> header_storage;
    std::vector<std::uint8_t> superblock_storage;
    sv::pba::PbaUnlockRequest request {};
    fill_unlock_request(
        container.value(),
        sv::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(wrong_password),
            std::strlen(wrong_password)),
        header_storage,
        superblock_storage,
        request);

    const auto unlocked = sv::pba::unlock_primary_header(request);
    REQUIRE_FALSE(unlocked.is_ok());
    REQUIRE(unlocked.error() == sv::ErrorCode::WrongCredentials);

    sv::test::cleanup_vault(path);
}