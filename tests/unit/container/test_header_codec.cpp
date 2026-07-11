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
    sb.outer_salt.fill(0x11);
    sb.hidden_salt.fill(0x22);
    return sb;
}

sv::container::HeaderPayload sample_payload() {
    sv::container::HeaderPayload payload {};
    payload.volume_master_key.fill(0x33);
    payload.header_key_confirm.fill(0x44);
    payload.manifest_offset = 0x10000;
    payload.manifest_size = 0x20000;
    payload.journal_offset = 0x3200;
    payload.journal_size = 0x8000;
    payload.data_offset = 0x50000;
    payload.data_size = 0x100000;
    payload.chunk_size = 64 * 1024;
    payload.keyfile_required = 1;
    payload.manifest_generation = 7;
    payload.manifest_root_hash.fill(0x55);
    payload.protect_margin_bytes = 1024 * 1024;
    return payload;
}

} // namespace

TEST_CASE("H-01 header payload encode/decode roundtrip", "[unit][container][h01]") {
    const auto payload = sample_payload();
    std::array<std::uint8_t, sv::container::SVLT_HEADER_PAYLOAD_SIZE> encoded {};
    REQUIRE(sv::container::encode_header_payload(payload, encoded).is_ok());

    const auto decoded = sv::container::decode_header_payload(encoded);
    REQUIRE(decoded.is_ok());
    REQUIRE(decoded.value().payload_version == payload.payload_version);
    REQUIRE(decoded.value().slot_type == payload.slot_type);
    REQUIRE(decoded.value().volume_master_key == payload.volume_master_key);
    REQUIRE(decoded.value().header_key_confirm == payload.header_key_confirm);
    REQUIRE(decoded.value().manifest_offset == payload.manifest_offset);
    REQUIRE(decoded.value().manifest_size == payload.manifest_size);
    REQUIRE(decoded.value().journal_offset == payload.journal_offset);
    REQUIRE(decoded.value().journal_size == payload.journal_size);
    REQUIRE(decoded.value().data_offset == payload.data_offset);
    REQUIRE(decoded.value().data_size == payload.data_size);
    REQUIRE(decoded.value().chunk_size == payload.chunk_size);
    REQUIRE(decoded.value().keyfile_required == payload.keyfile_required);
    REQUIRE(decoded.value().manifest_generation == payload.manifest_generation);
    REQUIRE(decoded.value().manifest_root_hash == payload.manifest_root_hash);
    REQUIRE(decoded.value().protect_margin_bytes == payload.protect_margin_bytes);
}

TEST_CASE("H-01 encrypted header sector encode/decode roundtrip", "[unit][container][h01]") {
    const auto sb = make_layout_superblock();
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const auto payload = sample_payload();
    std::array<std::uint8_t, 32> header_key {};
    header_key.fill(0x66);
    const auto sealed = sv::container::seal_header_sector(
        *crypto,
        payload,
        header_key,
        sb,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(sealed.is_ok());

    std::array<std::uint8_t, sv::container::SVLT_HEADER_SIZE> encoded {};
    REQUIRE(sv::container::encode_header_sector(sealed.value(), encoded).is_ok());
    const auto decoded_sector = sv::container::decode_header_sector(encoded);
    REQUIRE(decoded_sector.is_ok());

    const auto opened = sv::container::open_header_sector(
        *crypto,
        decoded_sector.value(),
        header_key,
        sb,
        sv::container::HeaderSlotId::PrimaryOuter,
        sv::container::HeaderSlotType::Outer);
    REQUIRE(opened.is_ok());
    REQUIRE(opened.value().manifest_generation == payload.manifest_generation);
    REQUIRE(opened.value().manifest_root_hash == payload.manifest_root_hash);
    REQUIRE(opened.value().volume_master_key == payload.volume_master_key);
}