#include <sv/exit_codes.hpp>
#include <sv/result.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace sv;

TEST_CASE("Result<T> ok and err", "[result]") {
    auto ok_result = Result<int>::ok(42);
    REQUIRE(ok_result.is_ok());
    REQUIRE(ok_result.value() == 42);
    REQUIRE(ok_result.error() == ErrorCode::Ok);
    REQUIRE(ok_result.message().empty());

    auto err_result = Result<int>::err(ErrorCode::IoError, "disk full");
    REQUIRE_FALSE(err_result.is_ok());
    REQUIRE(err_result.error() == ErrorCode::IoError);
    REQUIRE(err_result.message() == "disk full");
}

TEST_CASE("Result<void> ok and err", "[result]") {
    auto ok_result = Result<void>::ok();
    REQUIRE(ok_result.is_ok());
    REQUIRE_NOTHROW(ok_result.value());
    REQUIRE(ok_result.error() == ErrorCode::Ok);

    auto err_result = Result<void>::err(ErrorCode::VaultLocked, "unlock first");
    REQUIRE_FALSE(err_result.is_ok());
    REQUIRE(err_result.error() == ErrorCode::VaultLocked);
    REQUIRE(err_result.message() == "unlock first");
}

TEST_CASE("exit_code_for maps ErrorCode to CLI exit codes", "[result][exit_codes]") {
    REQUIRE(exit_code_for(ErrorCode::Ok) == exit_codes::success);
    REQUIRE(exit_code_for(ErrorCode::UsageError) == exit_codes::usage_error);
    REQUIRE(exit_code_for(ErrorCode::InvalidArgument) == exit_codes::usage_error);
    REQUIRE(exit_code_for(ErrorCode::WrongCredentials) == exit_codes::wrong_credentials);
    REQUIRE(exit_code_for(ErrorCode::HeaderDecryptFailed) == exit_codes::wrong_credentials);
    REQUIRE(exit_code_for(ErrorCode::CorruptContainer) == exit_codes::corrupt_container);
    REQUIRE(exit_code_for(ErrorCode::ManifestDecryptFailed) == exit_codes::corrupt_container);
    REQUIRE(exit_code_for(ErrorCode::HiddenVolumeProtected) == exit_codes::hidden_volume_protected);
    REQUIRE(exit_code_for(ErrorCode::VaultLocked) == exit_codes::vault_locked);
    REQUIRE(exit_code_for(ErrorCode::IoError) == exit_codes::io_error);
    REQUIRE(exit_code_for(ErrorCode::ManifestFull) == exit_codes::manifest_full);
    REQUIRE(exit_code_for(ErrorCode::NotImplemented) == exit_codes::general_error);
    REQUIRE(exit_code_for(ErrorCode::TransactionIncomplete) == exit_codes::general_error);
}

TEST_CASE("Result<T> value throws on error", "[result]") {
    auto err_result = Result<int>::err(ErrorCode::InvalidArgument);
    REQUIRE_THROWS_AS(err_result.value(), std::logic_error);
}