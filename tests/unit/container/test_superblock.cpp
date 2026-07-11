#include <sv/container/layout.hpp>
#include <sv/container/superblock.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

namespace {

sv::container::Superblock make_valid_superblock() {
    sv::container::CreateLayoutParams params {};
    params.container_size = 16ULL * 1024 * 1024;
    const auto layout = sv::container::compute_layout(params);
    REQUIRE(layout.is_ok());

    sv::container::Superblock sb {};
    std::memcpy(sb.magic.data(), sv::container::SVLT_MAGIC, 4);
    sb.flags = sv::container::SUPERBLOCK_FLAG_CREATED;
    sb.container_size = layout.value().container_size;
    sb.journal_offset = layout.value().journal_offset;
    sb.journal_size = layout.value().journal_size;
    sb.manifest_offset = layout.value().manifest_offset;
    sb.manifest_size = layout.value().manifest_size;
    sb.data_offset = layout.value().data_offset;
    sb.data_size = layout.value().data_size;
    sb.outer_salt.fill(0xAB);
    sb.hidden_salt.fill(0xCD);
    return sb;
}

} // namespace

TEST_CASE("H-04 superblock encodes to 512 bytes with valid CRC", "[unit][container][h04]") {
    const auto sb = make_valid_superblock();
    std::array<std::uint8_t, sv::container::SVLT_SUPERBLOCK_SIZE> encoded {};
    REQUIRE(sv::container::encode_superblock(sb, encoded).is_ok());
    REQUIRE(encoded.size() == sv::container::SVLT_SUPERBLOCK_SIZE);

    const auto decoded = sv::container::decode_superblock(encoded);
    REQUIRE(decoded.is_ok());
    REQUIRE(decoded.value().superblock_size == sv::container::SVLT_SUPERBLOCK_SIZE);
    REQUIRE(decoded.value().container_size == sb.container_size);
    REQUIRE(decoded.value().journal_offset == sb.journal_offset);
}

TEST_CASE("H-04 superblock CRC mismatch is rejected", "[unit][container][h04]") {
    const auto sb = make_valid_superblock();
    std::array<std::uint8_t, sv::container::SVLT_SUPERBLOCK_SIZE> encoded {};
    REQUIRE(sv::container::encode_superblock(sb, encoded).is_ok());
    encoded[0x68] ^= 0x01;

    const auto decoded = sv::container::decode_superblock(encoded);
    REQUIRE_FALSE(decoded.is_ok());
    REQUIRE(decoded.message() == "superblock crc mismatch");
}