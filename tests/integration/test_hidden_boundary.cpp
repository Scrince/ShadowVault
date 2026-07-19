#include "test_fixtures.hpp"

#include <sv/vault/hidden_volume.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

constexpr std::uint64_t kHiddenSize = 32ULL * 1024 * 1024;

std::uint64_t first_hidden_chunk_index(const sv::vault::UnlockedVault& session, std::uint64_t hidden_max_size) {
    const std::uint64_t hidden_offset = session.header.data_offset + session.header.data_size - hidden_max_size;
    const std::uint64_t chunk_slot = static_cast<std::uint64_t>(session.header.chunk_size) + sv::crypto::kAeadTagSize;
    return (hidden_offset - session.header.data_offset) / chunk_slot;
}

std::vector<std::uint8_t> bytes_of_size(std::size_t size) {
    return std::vector<std::uint8_t>(size, 0xCD);
}

} 

TEST_CASE("HV-04 outer put blocked at hidden offset via bitmap", "[integration][hidden][hv04]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv04");
    sv::vault::VaultEngine engine;
    const auto outer_cred = sv::test::outer_credentials();
    const auto hidden_cred = sv::test::hidden_credentials();
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, outer_cred, opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred, outer_opts);
    REQUIRE(outer.is_ok());
    REQUIRE(sv::vault::create_hidden_volume(engine, outer.value(), hidden_cred, kHiddenSize).is_ok());
    sv::test::release_session(outer.value());

    auto session = engine.open(path, outer_cred, outer_opts);
    REQUIRE(session.is_ok());

    const std::uint64_t first_hidden = first_hidden_chunk_index(session.value(), kHiddenSize);
    const std::size_t fill_size = static_cast<std::size_t>(first_hidden * opts.chunk_size);
    const auto fill = bytes_of_size(fill_size);
    const auto fill_put = engine.put(
        session.value(),
        "/decoy_fill.bin",
        sv::Span<const std::uint8_t>(fill.data(), fill.size()));
    if (!fill_put.is_ok()) {
        INFO("decoy fill error: " << fill_put.message());
    }
    REQUIRE(fill_put.is_ok());

    const auto overflow = engine.put(
        session.value(),
        "/decoy_overflow.bin",
        sv::Span<const std::uint8_t>(fill.data(), fill.size() + 1));
    REQUIRE_FALSE(overflow.is_ok());

    sv::test::cleanup_vault(path);
}

TEST_CASE("HV-08 create hidden when outer chunk overlaps", "[integration][hidden][hv08]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Hidden volume integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_hv08");
    sv::vault::VaultEngine engine;
    const auto outer_cred = sv::test::outer_credentials();
    const auto hidden_cred = sv::test::hidden_credentials();
    const auto opts = sv::test::hidden_test_create_options();

    REQUIRE(engine.create_new(path, outer_cred, opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred, outer_opts);
    REQUIRE(outer.is_ok());

    const std::uint64_t first_hidden = first_hidden_chunk_index(outer.value(), kHiddenSize);
    const std::size_t overlap_size = static_cast<std::size_t>((first_hidden + 1) * opts.chunk_size);
    const auto overlap = bytes_of_size(overlap_size);
    const auto put = engine.put(
        outer.value(),
        "/overlap.bin",
        sv::Span<const std::uint8_t>(overlap.data(), overlap.size()));
    REQUIRE(put.is_ok());

    const auto created = sv::vault::create_hidden_volume(engine, outer.value(), hidden_cred, kHiddenSize);
    REQUIRE_FALSE(created.is_ok());
    REQUIRE(created.error() == sv::ErrorCode::InvalidArgument);

    sv::test::cleanup_vault(path);
}