#include "test_fixtures.hpp"

#include <sv/crypto/crypto_provider.hpp>
#include <sv/recovery/journal.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

TEST_CASE("R-01 interrupted journal transaction aborts on reopen", "[integration][recovery][r01]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Recovery integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_r01");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const std::string payload = "survives interrupted transaction";
    REQUIRE(engine.put(
                session.value(),
                "/stable.txt",
                sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(payload.data()),
                    payload.size()))
                .is_ok());

    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);
    sv::recovery::Journal journal(
        session.value().container,
        *crypto,
        session.value().vmk.span(),
        session.value().superblock.journal_offset,
        session.value().superblock.journal_size);

    std::array<std::uint8_t, 16> transaction_id {};
    REQUIRE(crypto->random_bytes(sv::Span<std::uint8_t>(transaction_id.data(), transaction_id.size())).is_ok());
    REQUIRE(journal.append_begin(transaction_id, &session.value().superblock).is_ok());
    REQUIRE(session.value().container.sync().is_ok());
    sv::test::release_session(session.value());

    auto reopened = engine.open(path, sv::test::outer_credentials(), opts);
    if (!reopened.is_ok()) {
        FAIL(reopened.message());
    }

    const auto got = engine.get(reopened.value(), "/stable.txt");
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    sv::test::cleanup_vault(path);
}