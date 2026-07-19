#include <sv/container/header_codec.hpp>
#include <sv/container/layout.hpp>

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
    sb.outer_salt.fill(0x5A);
    sb.hidden_salt.fill(0xA5);
    return sb;
}

} 

TEST_CASE("H-03 primary and backup headers decrypt to identical payload", "[unit][container][h03]") {
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);
    const auto sb = make_layout_superblock();

    sv::container::HeaderPayload payload {};
    payload.volume_master_key.fill(0x01);
    payload.header_key_confirm.fill(0x02);
    payload.manifest_generation = 3;
    payload.manifest_root_hash.fill(0x03);
    payload.data_offset = sb.data_offset;
    payload.data_size = sb.data_size;

    std::array<std::uint8_t, 32> header_key {};
    header_key.fill(0x77);

    const auto primary = sv::container::seal_header_sector(
        *crypto,
        payload,
        header_key,
        sb,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(primary.is_ok());

    const auto backup = sv::container::seal_header_sector(
        *crypto,
        payload,
        header_key,
        sb,
        sv::container::HeaderSlotId::BackupOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(backup.is_ok());

    const auto opened_primary = sv::container::open_header_sector(
        *crypto,
        primary.value(),
        header_key,
        sb,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    const auto opened_backup = sv::container::open_header_sector(
        *crypto,
        backup.value(),
        header_key,
        sb,
        sv::container::HeaderSlotId::BackupOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(opened_primary.is_ok());
    REQUIRE(opened_backup.is_ok());
    REQUIRE(opened_primary.value().volume_master_key == opened_backup.value().volume_master_key);
    REQUIRE(opened_primary.value().manifest_root_hash == opened_backup.value().manifest_root_hash);
    REQUIRE(opened_primary.value().manifest_generation == opened_backup.value().manifest_generation);
}