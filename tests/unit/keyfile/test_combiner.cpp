#include <sv/keyfile/combiner.hpp>
#include <sv/keyfile/keyfile_format.hpp>
#include <sv/platform/file_io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>

namespace {

constexpr std::string_view kPassword = "integration-password-12";

sv::keyfile::KeyfileHash make_hash(const std::uint8_t byte) {
    sv::keyfile::KeyfileHash hash {};
    hash.fill(byte);
    return hash;
}

} // namespace

TEST_CASE("K-02 keyfile hash order changes kdf secret", "[unit][keyfile][k02]") {
    const auto hash_a = make_hash(0x11);
    const auto hash_b = make_hash(0x22);

    const std::array<sv::keyfile::KeyfileHash, 2> order_ab {hash_a, hash_b};
    const std::array<sv::keyfile::KeyfileHash, 2> order_ba {hash_b, hash_a};

    const auto secret_ab = sv::keyfile::combine_credentials(kPassword, order_ab);
    const auto secret_ba = sv::keyfile::combine_credentials(kPassword, order_ba);
    REQUIRE(secret_ab.is_ok());
    REQUIRE(secret_ba.is_ok());
    REQUIRE(secret_ab.value() != secret_ba.value());
}

TEST_CASE("K-02 path sort normalizes keyfile order", "[unit][keyfile][k02]") {
    const auto base = std::filesystem::temp_directory_path() / "sv_keyfile_sort";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);

    const std::filesystem::path key_a = base / "a.key";
    const std::filesystem::path key_b = base / "b.key";
    REQUIRE(sv::platform::write_file(
                key_a,
                sv::as_bytes(sv::Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>("alpha"), 5)))
                .is_ok());
    REQUIRE(sv::platform::write_file(
                key_b,
                sv::as_bytes(sv::Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>("bravo"), 5)))
                .is_ok());

    const auto forward = sv::keyfile::read_keyfiles({key_b, key_a});
    const auto reverse = sv::keyfile::read_keyfiles({key_a, key_b});
    REQUIRE(forward.is_ok());
    REQUIRE(reverse.is_ok());
    REQUIRE(forward.value().size() == 2);
    REQUIRE(reverse.value().size() == 2);
    REQUIRE(forward.value()[0].path == key_a);
    REQUIRE(forward.value()[1].path == key_b);
    REQUIRE(reverse.value()[0].hash == forward.value()[0].hash);
    REQUIRE(reverse.value()[1].hash == forward.value()[1].hash);

    std::array<sv::keyfile::KeyfileHash, 2> hashes {};
    hashes[0] = forward.value()[0].hash;
    hashes[1] = forward.value()[1].hash;
    const auto secret = sv::keyfile::combine_credentials(kPassword, hashes);
    REQUIRE(secret.is_ok());

    std::filesystem::remove_all(base, ec);
}