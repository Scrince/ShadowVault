#include <sv/container/header_codec.hpp>
#include <sv/container/layout.hpp>
#include <sv/container/superblock.hpp>
#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/pba/pba_header.hpp>
#include <sv/pba/pba_unlock.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/key_manager.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

namespace {

sv::container::Superblock make_layout_superblock() {
    sv::container::CreateLayoutParams params {};
    params.container_size = 16ULL * 1024 * 1024;
    const auto layout = sv::container::compute_layout(params);
    REQUIRE(layout.is_ok());

    sv::container::Superblock sb {};
    std::memcpy(sb.magic.data(), sv::container::SVLT_MAGIC, 4);
    sb.container_size = layout.value().container_size;
    sb.journal_offset = layout.value().journal_offset;
    sb.journal_size = layout.value().journal_size;
    sb.manifest_offset = layout.value().manifest_offset;
    sb.manifest_size = layout.value().manifest_size;
    sb.data_offset = layout.value().data_offset;
    sb.data_size = layout.value().data_size;
    sb.outer_salt.fill(0x11);
    sb.hidden_salt.fill(0x22);
    return sb;
}

sv::container::HeaderPayload sample_payload() {
    sv::container::HeaderPayload payload {};
    payload.volume_master_key.fill(0x33);
    payload.manifest_offset = 0x10000;
    payload.manifest_size = 0x20000;
    payload.journal_offset = 0x3200;
    payload.journal_size = 0x8000;
    payload.data_offset = 0x50000;
    payload.data_size = 0x100000;
    payload.chunk_size = 64 * 1024;
    payload.manifest_generation = 7;
    payload.manifest_root_hash.fill(0x55);
    payload.protect_margin_bytes = 1024 * 1024;
    return payload;
}

} 

TEST_CASE("P1-00 inspect header sector magic", "[unit][pba][p100]") {
    std::array<std::uint8_t, sv::container::SVLT_HEADER_SIZE> sector {};
    std::memcpy(sector.data(), sv::container::SVLT_MAGIC, sizeof(sv::container::SVLT_MAGIC));
    sector[4] = 1;
    sector[5] = 0;

    const auto view = sv::pba::inspect_header_sector(sector);
    REQUIRE(view.magic_ok);
    REQUIRE(view.format_version == 1);
}

TEST_CASE("P1-01 unlock_system_header rejects without superblock", "[unit][pba][p101]") {
    std::array<std::uint8_t, sv::container::SVLT_HEADER_SIZE> sector {};
    std::memcpy(sector.data(), sv::container::SVLT_MAGIC, sizeof(sv::container::SVLT_MAGIC));

    sv::pba::PbaCredentials cred {};
    const std::uint8_t pw[] = {'t', 'e', 's', 't'};
    cred.password_utf8 = sv::Span<const std::uint8_t>(pw, sizeof(pw));

    const auto unlocked = sv::pba::unlock_system_header(sector, cred);
    REQUIRE_FALSE(unlocked.is_ok());
    REQUIRE(unlocked.error() == sv::ErrorCode::InvalidArgument);
}

TEST_CASE("P1-02 unlock_primary_header VMK roundtrip", "[unit][pba][p102]") {
    auto sb = make_layout_superblock();
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const char* password = "pba-unlock-test";
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = password;

    auto kdf = sv::vault::default_kdf_params_portable();
    kdf.m_cost_kib = 4096;
    sv::vault::store_outer_kdf_hint(sb, kdf);

    sv::vault::KeyManager keys(*crypto);
    const auto derived = keys.derive_from_credentials(cred, sb, sv::vault::HeaderSlotIntent::Outer, kdf);
    REQUIRE(derived.is_ok());

    auto payload = sample_payload();
    const auto confirm = keys.compute_header_key_confirm(derived.value().header_key.span());
    REQUIRE(confirm.is_ok());
    payload.header_key_confirm = confirm.value();

    const auto sealed = keys.seal_header(
        payload,
        derived.value(),
        sb,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(sealed.is_ok());

    std::array<std::uint8_t, sv::container::SVLT_HEADER_SIZE> header_bytes {};
    REQUIRE(sv::container::encode_header_sector(sealed.value(), header_bytes).is_ok());

    std::array<std::uint8_t, sv::container::SVLT_SUPERBLOCK_SIZE> superblock_bytes {};
    REQUIRE(sv::container::encode_superblock(sb, superblock_bytes).is_ok());

    sv::pba::PbaUnlockRequest request {};
    request.primary_header_sector = sv::Span<const std::uint8_t>(header_bytes.data(), header_bytes.size());
    request.superblock_bytes = sv::Span<const std::uint8_t>(superblock_bytes.data(), superblock_bytes.size());
    request.credentials.password_utf8 = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(password),
        std::strlen(password));

    const auto unlocked = sv::pba::unlock_primary_header(request);
    REQUIRE(unlocked.is_ok());
    REQUIRE(unlocked.value() == payload.volume_master_key);
}

TEST_CASE("P1-03 wrong passphrase returns uniform credential error", "[unit][pba][p103]") {
    auto sb = make_layout_superblock();
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const char* password = "correct-horse";
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = password;

    auto kdf = sv::vault::default_kdf_params_portable();
    kdf.m_cost_kib = 4096;
    sv::vault::store_outer_kdf_hint(sb, kdf);

    sv::vault::KeyManager keys(*crypto);
    const auto derived = keys.derive_from_credentials(cred, sb, sv::vault::HeaderSlotIntent::Outer, kdf);
    REQUIRE(derived.is_ok());

    auto payload = sample_payload();
    const auto confirm = keys.compute_header_key_confirm(derived.value().header_key.span());
    REQUIRE(confirm.is_ok());
    payload.header_key_confirm = confirm.value();

    const auto sealed = keys.seal_header(
        payload,
        derived.value(),
        sb,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(sealed.is_ok());

    std::array<std::uint8_t, sv::container::SVLT_HEADER_SIZE> header_bytes {};
    REQUIRE(sv::container::encode_header_sector(sealed.value(), header_bytes).is_ok());

    std::array<std::uint8_t, sv::container::SVLT_SUPERBLOCK_SIZE> superblock_bytes {};
    REQUIRE(sv::container::encode_superblock(sb, superblock_bytes).is_ok());

    const char* wrong_password = "wrong-password";
    sv::pba::PbaUnlockRequest request {};
    request.primary_header_sector = sv::Span<const std::uint8_t>(header_bytes.data(), header_bytes.size());
    request.superblock_bytes = sv::Span<const std::uint8_t>(superblock_bytes.data(), superblock_bytes.size());
    request.credentials.password_utf8 = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(wrong_password),
        std::strlen(wrong_password));

    const auto unlocked = sv::pba::unlock_primary_header(request);
    REQUIRE_FALSE(unlocked.is_ok());
    REQUIRE(unlocked.error() == sv::ErrorCode::WrongCredentials);
}