#include "test_fixtures.hpp"

#include <sv/keyfile/keyfile_format.hpp>
#include <sv/platform/file_io.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

std::filesystem::path write_temp_keyfile(const std::string& name, const std::string& contents) {
    const auto path = std::filesystem::path("C:/Temp") / name;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    REQUIRE(sv::platform::write_file(
                path,
                sv::as_bytes(sv::Span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(contents.data()),
                    contents.size())))
                .is_ok());
    return path;
}

sv::vault::CredentialMaterial cred_with_keyfile(const std::filesystem::path& keyfile_path) {
    auto cred = sv::test::outer_credentials();
    const auto entry = sv::keyfile::read_keyfile(keyfile_path);
    REQUIRE(entry.is_ok());
    cred.keyfiles.push_back(entry.value());
    return cred;
}

} // namespace

TEST_CASE("K-01 single keyfile unlock", "[integration][keyfile][k01]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Keyfile integration tests run in Release builds");
    }

    const auto key_path = write_temp_keyfile("sv_test_k01.key", "keyfile-payload-k01");
    const auto vault_path = sv::test::temp_vault_path("sv_test_k01");
    const auto cred = cred_with_keyfile(key_path);

    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, cred, sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    auto session = engine.open(vault_path, cred, opts);
    REQUIRE(session.is_ok());
    REQUIRE(session.value().header.keyfile_required == 1);
    sv::test::release_session(session.value());

    std::error_code ec;
    std::filesystem::remove(key_path, ec);
    sv::test::cleanup_vault(vault_path);
}

TEST_CASE("K-03 keyfile_required enforcement", "[integration][keyfile][k03]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());
    if (sv::test::integration_release_only()) {
        SKIP("Keyfile integration tests run in Release builds");
    }

    const auto key_path = write_temp_keyfile("sv_test_k03.key", "keyfile-payload-k03");
    const auto vault_path = sv::test::temp_vault_path("sv_test_k03");
    const auto cred = cred_with_keyfile(key_path);

    sv::vault::VaultEngine engine;
    REQUIRE(engine.create_new(vault_path, cred, sv::test::file_ops_create_options()).is_ok());

    sv::vault::OpenVaultOptions opts {};
    const auto password_only = engine.open(vault_path, sv::test::outer_credentials(), opts);
    REQUIRE_FALSE(password_only.is_ok());
    REQUIRE(password_only.message() == "keyfile required");

    std::error_code ec;
    std::filesystem::remove(key_path, ec);
    sv::test::cleanup_vault(vault_path);
}