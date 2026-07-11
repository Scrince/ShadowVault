#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("F-08 unicode paths UTF-8", "[integration][file][f08]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("File ops integration tests run in Release builds");
    }

    const auto path = sv::test::temp_vault_path("sv_test_f08");
    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(path, sv::test::outer_credentials(), sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(path, sv::test::outer_credentials(), opts);
    REQUIRE(session.is_ok());

    const std::string dir = "/\xe6\x96\x87\xe6\x91\xa3/\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
    const std::string file = dir + "/\xe3\x83\x95\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab.txt";
    const std::string payload = "Unicode \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xf0\x9f\x94\x92";

    REQUIRE(engine.mkdir(session.value(), dir).is_ok());
    REQUIRE(engine.put(
                session.value(),
                file,
                sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(payload.data()),
                    payload.size()))
                .is_ok());

    const auto got = engine.get(session.value(), file);
    REQUIRE(got.is_ok());
    REQUIRE(std::string(got.value().begin(), got.value().end()) == payload);

    sv::test::cleanup_vault(path);
}