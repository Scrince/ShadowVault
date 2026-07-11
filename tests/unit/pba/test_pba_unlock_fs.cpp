#include <sv/container/header_codec.hpp>
#include <sv/container/layout.hpp>
#include <sv/container/superblock.hpp>
#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/pba/pba_unlock.hpp>
#include <sv/pba/pba_unlock_fs.h>
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

} // namespace

TEST_CASE("P1-06 freestanding C API matches C++ unlock_primary_header", "[unit][pba][p106]") {
    auto sb = make_layout_superblock();
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const char* password = "freestanding-parity";
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = password;

    auto kdf = sv::vault::default_kdf_params_portable();
    kdf.m_cost_kib = 4096;
    sv::vault::store_outer_kdf_hint(sb, kdf);

    sv::vault::KeyManager keys(*crypto);
    const auto derived = keys.derive_from_credentials(cred, sb, sv::vault::HeaderSlotIntent::Outer, kdf);
    REQUIRE(derived.is_ok());

    sv::container::HeaderPayload payload {};
    payload.volume_master_key.fill(0xAB);
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

    sv::pba::PbaUnlockRequest cpp_request {};
    cpp_request.primary_header_sector = sv::Span<const std::uint8_t>(header_bytes.data(), header_bytes.size());
    cpp_request.superblock_bytes = sv::Span<const std::uint8_t>(superblock_bytes.data(), superblock_bytes.size());
    cpp_request.credentials.password_utf8 = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(password),
        std::strlen(password));

    const auto cpp_unlocked = sv::pba::unlock_primary_header(cpp_request);
    REQUIRE(cpp_unlocked.is_ok());

    sv_pba_unlock_request_t c_request {};
    c_request.primary_header_sector = header_bytes.data();
    c_request.primary_header_sector_len = header_bytes.size();
    c_request.superblock_bytes = superblock_bytes.data();
    c_request.superblock_bytes_len = superblock_bytes.size();
    c_request.password_utf8 = reinterpret_cast<const std::uint8_t*>(password);
    c_request.password_utf8_len = std::strlen(password);

    std::array<std::uint8_t, 32> c_vmk {};
    const auto c_status = sv_pba_unlock_primary_header(&c_request, c_vmk.data());
    REQUIRE(c_status == SV_PBA_STATUS_OK);
    REQUIRE(c_vmk == cpp_unlocked.value());
    REQUIRE(c_vmk == payload.volume_master_key);
}