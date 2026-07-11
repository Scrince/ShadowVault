#include "test_fixtures.hpp"

#include <sv/vault/hidden_volume.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kHiddenSize = 32ULL * 1024 * 1024;

std::uint64_t chunk_slot_bytes(const sv::vault::UnlockedVault& session) {
    return static_cast<std::uint64_t>(session.header.chunk_size) + sv::crypto::kAeadTagSize;
}

std::uint64_t write_end_for_chunks(
    const sv::vault::UnlockedVault& session,
    std::uint64_t first_chunk,
    std::uint32_t chunk_count) {
    return session.header.data_offset
        + (first_chunk + chunk_count) * chunk_slot_bytes(session);
}

std::vector<std::uint8_t> bytes_of_size(std::size_t size, std::uint8_t fill = 0xAB) {
    return std::vector<std::uint8_t>(size, fill);
}

} // namespace

TEST_CASE("HV-03 outer put near boundary fails with protect", "[integration][hidden][hv03]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv03");
    sv::vault::VaultEngine engine;
    const auto outer_cred = sv::test::outer_credentials();
    const auto hidden_cred = sv::test::hidden_credentials();
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, outer_cred, opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred, outer_opts);
    REQUIRE(outer.is_ok());
    REQUIRE(sv::vault::create_hidden_volume(engine, outer.value(), hidden_cred, kHiddenSize).is_ok());

    const std::uint64_t hidden_offset = outer.value().header.hidden_offset;
    const std::uint64_t margin = outer.value().header.protect_margin_bytes;
    REQUIRE(margin > 0);
    const std::uint64_t safe_end = hidden_offset > margin ? hidden_offset - margin : 0;

    std::uint64_t target_chunks = 1;
    while (write_end_for_chunks(outer.value(), 0, static_cast<std::uint32_t>(target_chunks)) <= safe_end) {
        ++target_chunks;
    }
    REQUIRE(target_chunks > 1);

    const std::size_t payload_size = static_cast<std::size_t>((target_chunks - 1) * opts.chunk_size + 1);
    const auto payload = bytes_of_size(payload_size);
    sv::test::release_session(outer.value());

    sv::vault::OpenVaultOptions protect_opts {};
    protect_opts.protect_hidden = true;
    auto protected_outer = engine.open(path, outer_cred, protect_opts);
    REQUIRE(protected_outer.is_ok());

    const auto blocked = engine.put(
        protected_outer.value(),
        "/margin_probe.bin",
        sv::Span<const std::uint8_t>(payload.data(), payload.size()));
    REQUIRE_FALSE(blocked.is_ok());
    REQUIRE(blocked.error() == sv::ErrorCode::HiddenVolumeProtected);
    sv::test::release_session(protected_outer.value());

    sv::vault::OpenVaultOptions plain_opts {};
    auto plain_outer = engine.open(path, outer_cred, plain_opts);
    REQUIRE(plain_outer.is_ok());
    const auto allowed = engine.put(
        plain_outer.value(),
        "/margin_probe.bin",
        sv::Span<const std::uint8_t>(payload.data(), payload.size()));
    if (!allowed.is_ok()) {
        INFO("expected allow without protect: " << allowed.message());
    }
    REQUIRE(allowed.is_ok());
    sv::test::release_session(plain_outer.value());

    sv::test::cleanup_vault(path);
}