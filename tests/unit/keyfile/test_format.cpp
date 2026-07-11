#include <sv/keyfile/keyfile_format.hpp>
#include <sv/platform/file_io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

TEST_CASE("K-04 keyfile larger than 512 KiB is rejected", "[unit][keyfile][k04]") {
    const auto path = std::filesystem::temp_directory_path() / "sv_keyfile_oversize.bin";
    std::vector<std::uint8_t> data(sv::keyfile::kMaxKeyfileReadSize + 1, 0xAB);
    REQUIRE(sv::platform::write_file(path, sv::as_bytes(sv::Span<const std::uint8_t>(data.data(), data.size()))).is_ok());

    const auto entry = sv::keyfile::read_keyfile(path);
    REQUIRE_FALSE(entry.is_ok());

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("K-04 keyfile at 512 KiB limit is accepted", "[unit][keyfile][k04]") {
    const auto path = std::filesystem::temp_directory_path() / "sv_keyfile_max.bin";
    std::vector<std::uint8_t> data(sv::keyfile::kMaxKeyfileReadSize, 0xCD);
    REQUIRE(sv::platform::write_file(path, sv::as_bytes(sv::Span<const std::uint8_t>(data.data(), data.size()))).is_ok());

    const auto entry = sv::keyfile::read_keyfile(path);
    REQUIRE(entry.is_ok());
    REQUIRE(entry.value().path == path);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}