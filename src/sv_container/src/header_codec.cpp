#include <sv/container/header_codec.hpp>

#include <sv/crc32.hpp>
#include <sv/endian.hpp>

#include <cstring>

namespace sv::container {

namespace {

constexpr std::size_t kHeaderSlotCrcCoverage = 4080;

void write_array(std::uint8_t* out, std::size_t offset, Span<const std::uint8_t> bytes) noexcept {
    std::memcpy(out + offset, bytes.data(), bytes.size());
}

} // namespace

Result<std::array<std::uint8_t, SVLT_AAD_SIZE>> build_header_aad(
    const Superblock& sb,
    const HeaderSlotId slot_id,
    const HeaderSlotType slot_type) noexcept {
    std::array<std::uint8_t, SVLT_AAD_SIZE> aad {};
    std::memcpy(aad.data(), SVLT_MAGIC, 4);
    write_le16(aad.data() + 4, sb.format_version);
    write_le16(aad.data() + 6, static_cast<std::uint16_t>(slot_id));
    aad[8] = static_cast<std::uint8_t>(slot_type);
    std::memcpy(aad.data() + 16, sb.outer_salt.data(), sb.outer_salt.size());
    std::memcpy(aad.data() + 48, sb.hidden_salt.data(), sb.hidden_salt.size());
    return Result<std::array<std::uint8_t, SVLT_AAD_SIZE>>::ok(aad);
}

Result<void> encode_header_payload(const HeaderPayload& payload, Span<std::uint8_t> out) noexcept {
    if (out.size() < SVLT_HEADER_PAYLOAD_SIZE) {
        return Result<void>::err(ErrorCode::InvalidArgument, "header payload buffer too small");
    }
    std::memset(out.data(), 0, out.size());
    write_le16(out.data() + 0x00, payload.payload_version);
    out[0x02] = payload.slot_type;
    out[0x03] = payload.kdf_type;
    write_array(out.data(), 0x04, payload.volume_master_key);
    write_array(out.data(), 0x24, payload.header_key_confirm);
    write_le64(out.data() + 0x44, payload.manifest_offset);
    write_le64(out.data() + 0x4C, payload.manifest_size);
    write_le64(out.data() + 0x54, payload.journal_offset);
    write_le64(out.data() + 0x5C, payload.journal_size);
    write_le64(out.data() + 0x64, payload.data_offset);
    write_le64(out.data() + 0x6C, payload.data_size);
    write_le32(out.data() + 0x74, payload.chunk_size);
    out[0x78] = payload.aead_id;
    out[0x79] = payload.keyfile_required;
    write_array(out.data(), 0x7C, payload.kdf_params_blob);
    write_le64(out.data() + 0xBC, payload.manifest_generation);
    write_le64(out.data() + 0xC4, payload.journal_sequence);
    write_array(out.data(), 0xCC, payload.manifest_root_hash);
    write_le64(out.data() + 0xEC, payload.hidden_offset);
    write_le64(out.data() + 0xF4, payload.hidden_max_size);
    out[0xFC] = payload.hidden_present;
    write_le64(out.data() + 0x100, payload.protect_margin_bytes);
    write_le64(out.data() + 0x108, payload.hidden_journal_offset);
    write_le64(out.data() + 0x110, payload.hidden_journal_size);
    write_le64(out.data() + 0x118, payload.hidden_manifest_offset);
    write_le64(out.data() + 0x120, payload.hidden_manifest_size);
    write_le64(out.data() + 0x128, payload.hidden_data_offset);
    write_le64(out.data() + 0x130, payload.hidden_data_size);
    return Result<void>::ok();
}

Result<HeaderPayload> decode_header_payload(Span<const std::uint8_t> data) noexcept {
    if (data.size() < SVLT_HEADER_PAYLOAD_SIZE) {
        return Result<HeaderPayload>::err(ErrorCode::CorruptContainer, "header payload too short");
    }

    HeaderPayload payload {};
    payload.payload_version = read_le16(data.data() + 0x00);
    payload.slot_type = data[0x02];
    payload.kdf_type = data[0x03];
    std::memcpy(payload.volume_master_key.data(), data.data() + 0x04, payload.volume_master_key.size());
    std::memcpy(payload.header_key_confirm.data(), data.data() + 0x24, payload.header_key_confirm.size());
    payload.manifest_offset = read_le64(data.data() + 0x44);
    payload.manifest_size = read_le64(data.data() + 0x4C);
    payload.journal_offset = read_le64(data.data() + 0x54);
    payload.journal_size = read_le64(data.data() + 0x5C);
    payload.data_offset = read_le64(data.data() + 0x64);
    payload.data_size = read_le64(data.data() + 0x6C);
    payload.chunk_size = read_le32(data.data() + 0x74);
    payload.aead_id = data[0x78];
    payload.keyfile_required = data[0x79];
    std::memcpy(payload.kdf_params_blob.data(), data.data() + 0x7C, payload.kdf_params_blob.size());
    payload.manifest_generation = read_le64(data.data() + 0xBC);
    payload.journal_sequence = read_le64(data.data() + 0xC4);
    std::memcpy(payload.manifest_root_hash.data(), data.data() + 0xCC, payload.manifest_root_hash.size());
    payload.hidden_offset = read_le64(data.data() + 0xEC);
    payload.hidden_max_size = read_le64(data.data() + 0xF4);
    payload.hidden_present = data[0xFC];
    payload.protect_margin_bytes = read_le64(data.data() + 0x100);
    payload.hidden_journal_offset = read_le64(data.data() + 0x108);
    payload.hidden_journal_size = read_le64(data.data() + 0x110);
    payload.hidden_manifest_offset = read_le64(data.data() + 0x118);
    payload.hidden_manifest_size = read_le64(data.data() + 0x120);
    payload.hidden_data_offset = read_le64(data.data() + 0x128);
    payload.hidden_data_size = read_le64(data.data() + 0x130);
    return Result<HeaderPayload>::ok(payload);
}

Result<void> encode_header_sector(const EncryptedHeaderSector& sector, Span<std::uint8_t> out) noexcept {
    if (out.size() < SVLT_HEADER_SIZE) {
        return Result<void>::err(ErrorCode::InvalidArgument, "header sector buffer too small");
    }
    std::memset(out.data(), 0, out.size());
    write_array(out.data(), 0, sector.header_nonce);
    write_array(out.data(), 24, sector.ciphertext);
    write_array(out.data(), 3576, sector.tag);
    write_le32(out.data() + 4080, sector.header_slot_crc32);
    return Result<void>::ok();
}

Result<EncryptedHeaderSector> decode_header_sector(Span<const std::uint8_t> data) noexcept {
    if (data.size() < SVLT_HEADER_SIZE) {
        return Result<EncryptedHeaderSector>::err(ErrorCode::CorruptContainer, "header sector too short");
    }

    EncryptedHeaderSector sector {};
    std::memcpy(sector.header_nonce.data(), data.data(), sector.header_nonce.size());
    std::memcpy(sector.ciphertext.data(), data.data() + 24, sector.ciphertext.size());
    std::memcpy(sector.tag.data(), data.data() + 3576, sector.tag.size());
    sector.header_slot_crc32 = read_le32(data.data() + 4080);

    const std::uint32_t expected_crc = crc32_ieee(data.subspan(0, kHeaderSlotCrcCoverage));
    if (sector.header_slot_crc32 != expected_crc) {
        return Result<EncryptedHeaderSector>::err(ErrorCode::CorruptContainer, "header_slot_crc32 mismatch");
    }

    return Result<EncryptedHeaderSector>::ok(sector);
}

Result<EncryptedHeaderSector> seal_header_sector(
    crypto::CryptoProvider& crypto,
    const HeaderPayload& payload,
    Span<const std::uint8_t> header_key,
    const Superblock& sb,
    const HeaderSlotId slot_id,
    const HeaderSlotType slot_type) noexcept {
    if (header_key.size() != crypto::kAeadKeySize) {
        return Result<EncryptedHeaderSector>::err(ErrorCode::InvalidArgument, "header_key must be 32 bytes");
    }

    auto aad_result = build_header_aad(sb, slot_id, slot_type);
    if (!aad_result.is_ok()) {
        return Result<EncryptedHeaderSector>::err(aad_result.error(), aad_result.message());
    }

    std::array<std::uint8_t, SVLT_HEADER_PAYLOAD_SIZE> plaintext {};
    const auto encode = encode_header_payload(payload, plaintext);
    if (!encode.is_ok()) {
        return Result<EncryptedHeaderSector>::err(encode.error(), encode.message());
    }

    EncryptedHeaderSector sector {};
    const auto nonce_result = crypto.random_bytes(sector.header_nonce);
    if (!nonce_result.is_ok()) {
        return Result<EncryptedHeaderSector>::err(nonce_result.error(), nonce_result.message());
    }

    crypto::AeadKey key {};
    std::memcpy(key.bytes.data(), header_key.data(), header_key.size());
    crypto::AeadNonce nonce {};
    std::memcpy(nonce.bytes.data(), sector.header_nonce.data(), sector.header_nonce.size());

    const auto enc = crypto.aead_encrypt(
        plaintext,
        aad_result.value(),
        key,
        nonce,
        sector.ciphertext,
        sector.tag);
    if (!enc.is_ok()) {
        return Result<EncryptedHeaderSector>::err(enc.error(), enc.message());
    }

    std::array<std::uint8_t, SVLT_HEADER_SIZE> encoded {};
    const auto encode_sector = encode_header_sector(sector, encoded);
    if (!encode_sector.is_ok()) {
        return Result<EncryptedHeaderSector>::err(encode_sector.error(), encode_sector.message());
    }
    sector.header_slot_crc32 = crc32_ieee(
        Span<const std::uint8_t>(encoded.data(), kHeaderSlotCrcCoverage));
    return Result<EncryptedHeaderSector>::ok(sector);
}

Result<HeaderPayload> open_header_sector(
    crypto::CryptoProvider& crypto,
    const EncryptedHeaderSector& sector,
    Span<const std::uint8_t> header_key,
    const Superblock& sb,
    const HeaderSlotId slot_id,
    const HeaderSlotType slot_type) noexcept {
    if (header_key.size() != crypto::kAeadKeySize) {
        return Result<HeaderPayload>::err(ErrorCode::InvalidArgument, "header_key must be 32 bytes");
    }

    auto aad_result = build_header_aad(sb, slot_id, slot_type);
    if (!aad_result.is_ok()) {
        return Result<HeaderPayload>::err(aad_result.error(), aad_result.message());
    }

    crypto::AeadKey key {};
    std::memcpy(key.bytes.data(), header_key.data(), header_key.size());
    crypto::AeadNonce nonce {};
    std::memcpy(nonce.bytes.data(), sector.header_nonce.data(), sector.header_nonce.size());

    std::array<std::uint8_t, SVLT_HEADER_PAYLOAD_SIZE> plaintext {};
    const auto dec = crypto.aead_decrypt(
        sector.ciphertext,
        aad_result.value(),
        key,
        nonce,
        sector.tag,
        plaintext);
    if (!dec.is_ok()) {
        return Result<HeaderPayload>::err(ErrorCode::WrongCredentials, "header decrypt failed");
    }

    return decode_header_payload(plaintext);
}

} // namespace sv::container