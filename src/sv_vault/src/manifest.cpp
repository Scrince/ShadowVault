#include <sv/vault/manifest.hpp>

#include <sv/crypto/hkdf.hpp>
#include <sv/endian.hpp>

#include <sodium.h>

#include <cstring>

namespace sv::vault {

namespace {

constexpr std::array<std::uint8_t, 4> kPageMagic = {'S', 'V', 'M', 'P'};
[[nodiscard]] std::array<std::uint8_t, 24> derive_manifest_nonce(
    const std::uint64_t generation,
    const std::uint64_t page_index) noexcept {
    std::array<std::uint8_t, 24> input {};
    std::memcpy(input.data(), "sv-mn-v1", 8);
    write_le64(input.data() + 8, generation);
    write_le64(input.data() + 16, page_index);

    std::array<std::uint8_t, 24> nonce {};
    crypto_generichash_blake2b(nonce.data(), nonce.size(), input.data(), input.size(), nullptr, 0);
    return nonce;
}

[[nodiscard]] bool digest_page_material(
    const ManifestPage& page,
    std::array<std::uint8_t, 32>& out) noexcept {
    const std::size_t per_page_size = 24 + page.body.size();
    std::vector<std::uint8_t> per_page(per_page_size);
    std::size_t pos = 0;
    write_le16(per_page.data() + pos, static_cast<std::uint16_t>(page.header.page_type));
    pos += 2;
    write_le16(per_page.data() + pos, page.header.page_version);
    pos += 2;
    write_le64(per_page.data() + pos, page.header.generation);
    pos += 8;
    write_le64(per_page.data() + pos, page.header.page_index);
    pos += 8;
    write_le32(per_page.data() + pos, page.header.entry_count);
    pos += 4;
    if (!page.body.empty()) {
        std::memcpy(per_page.data() + pos, page.body.data(), page.body.size());
    }
    return crypto_generichash_blake2b(out.data(), out.size(), per_page.data(), per_page.size(), nullptr, 0) == 0;
}

} 

Result<void> encode_manifest_page_header(const ManifestPageHeader& header, Span<std::uint8_t> out) noexcept {
    if (out.size() < kManifestPageHeaderSize) {
        return Result<void>::err(ErrorCode::InvalidArgument, "manifest page header buffer too small");
    }
    std::memset(out.data(), 0, out.size());
    std::memcpy(out.data(), header.page_magic.data(), header.page_magic.size());
    write_le16(out.data() + 4, header.page_version);
    write_le16(out.data() + 6, static_cast<std::uint16_t>(header.page_type));
    write_le64(out.data() + 8, header.generation);
    write_le64(out.data() + 16, header.page_index);
    write_le32(out.data() + 24, header.entry_count);
    return Result<void>::ok();
}

Result<ManifestPageHeader> decode_manifest_page_header(Span<const std::uint8_t> data) noexcept {
    if (data.size() < kManifestPageHeaderSize) {
        return Result<ManifestPageHeader>::err(ErrorCode::CorruptContainer, "manifest page header too short");
    }

    ManifestPageHeader header {};
    std::memcpy(header.page_magic.data(), data.data(), header.page_magic.size());
    if (header.page_magic != kPageMagic) {
        return Result<ManifestPageHeader>::err(ErrorCode::CorruptContainer, "invalid manifest page magic");
    }
    header.page_version = read_le16(data.data() + 4);
    header.page_type = static_cast<container::ManifestPageType>(read_le16(data.data() + 6));
    header.generation = read_le64(data.data() + 8);
    header.page_index = read_le64(data.data() + 16);
    header.entry_count = read_le32(data.data() + 24);
    return Result<ManifestPageHeader>::ok(header);
}

std::uint64_t chunk_count_for_data(const std::uint64_t data_size, const std::uint32_t chunk_size) noexcept {
    if (chunk_size == 0) {
        return 0;
    }
    return (data_size + chunk_size - 1) / chunk_size;
}

std::size_t bitmap_byte_count(const std::uint64_t data_size, const std::uint32_t chunk_size) noexcept {
    const std::uint64_t chunks = chunk_count_for_data(data_size, chunk_size);
    return static_cast<std::size_t>((chunks * 2 + 7) / 8);
}

Result<ManifestState> create_empty_manifest(const std::uint64_t data_size, const std::uint32_t chunk_size) noexcept {
    ManifestState state {};
    state.generation = 1;
    const std::size_t bitmap_size = bitmap_byte_count(data_size, chunk_size);

    Inode root {};
    root.entry_type = container::InodeEntryType::Directory;
    root.name_utf8 = "";
    root.parent_id = 0;
    root.inode_id = 1;
    root.mtime_unix = 0;
    state.inodes.push_back(root);

    state.bitmap.assign(bitmap_size, 0);
    if (bitmap_size > 16 * 1024 * 1024) {
        return Result<ManifestState>::err(ErrorCode::InvalidArgument, "bitmap too large for v1 manifest");
    }
    const auto hash = compute_manifest_root_hash(state);
    if (!hash.is_ok()) {
        return Result<ManifestState>::err(hash.error(), hash.message());
    }
    state.root_hash = hash.value();
    return Result<ManifestState>::ok(std::move(state));
}

Result<std::array<std::uint8_t, 32>> compute_manifest_root_hash(const ManifestState& state) noexcept {
    std::array<std::uint8_t, 32> root {};
    {
        const auto pages = manifest_to_pages(state);
        if (!pages.is_ok()) {
            return Result<std::array<std::uint8_t, 32>>::err(pages.error(), pages.message());
        }

        const auto& page_pair = pages.value();
        std::array<std::uint8_t, 64> page_digests {};
        std::size_t digest_offset = 0;
        for (const ManifestPage* page : {&page_pair.first, &page_pair.second}) {
            std::array<std::uint8_t, 32> digest {};
            if (!digest_page_material(*page, digest)) {
                return Result<std::array<std::uint8_t, 32>>::err(
                    ErrorCode::PlatformError, "manifest page digest failed");
            }
            std::memcpy(page_digests.data() + digest_offset, digest.data(), digest.size());
            digest_offset += digest.size();
        }

        if (crypto_generichash_blake2b(
                root.data(),
                root.size(),
                page_digests.data(),
                digest_offset,
                nullptr,
                0)
            != 0) {
            return Result<std::array<std::uint8_t, 32>>::err(ErrorCode::PlatformError, "manifest root digest failed");
        }
    }
    return Result<std::array<std::uint8_t, 32>>::ok(root);
}

Result<std::pair<ManifestPage, ManifestPage>> manifest_to_pages(const ManifestState& state) noexcept {
    if (state.inodes.size() > container::SVLT_MAX_INODES_V1) {
        return Result<std::pair<ManifestPage, ManifestPage>>::err(ErrorCode::ManifestFull, "inode table full");
    }

    ManifestPage inode_page {};
    inode_page.header.page_type = container::ManifestPageType::InodeTable;
    inode_page.header.generation = state.generation;
    inode_page.header.page_index = 0;
    inode_page.header.entry_count = static_cast<std::uint32_t>(state.inodes.size());
    inode_page.body.resize(state.inodes.size() * container::SVLT_INODE_SIZE);

    std::size_t offset = 0;
    for (const auto& inode : state.inodes) {
        const auto enc = encode_inode(inode, Span<std::uint8_t>(inode_page.body.data() + offset, container::SVLT_INODE_SIZE));
        if (!enc.is_ok()) {
            return Result<std::pair<ManifestPage, ManifestPage>>::err(enc.error(), enc.message());
        }
        offset += container::SVLT_INODE_SIZE;
    }

    ManifestPage bitmap_page {};
    bitmap_page.header.page_type = container::ManifestPageType::FreeBitmap;
    bitmap_page.header.generation = state.generation;
    bitmap_page.header.page_index = 1;
    bitmap_page.header.entry_count = static_cast<std::uint32_t>(state.bitmap.size());
    bitmap_page.body = state.bitmap;

    return Result<std::pair<ManifestPage, ManifestPage>>::ok(std::make_pair(std::move(inode_page), std::move(bitmap_page)));
}

Result<ManifestState> manifest_from_pages(const ManifestPage& inode_page, const ManifestPage& bitmap_page) noexcept {
    ManifestState state {};
    state.generation = inode_page.header.generation;
    if (bitmap_page.header.generation != state.generation) {
        return Result<ManifestState>::err(ErrorCode::CorruptContainer, "manifest generation mismatch");
    }

    state.inodes.reserve(inode_page.header.entry_count);
    if (inode_page.body.size() < inode_page.header.entry_count * container::SVLT_INODE_SIZE) {
        return Result<ManifestState>::err(ErrorCode::CorruptContainer, "inode page body too short");
    }

    for (std::uint32_t i = 0; i < inode_page.header.entry_count; ++i) {
        const auto offset = static_cast<std::size_t>(i) * container::SVLT_INODE_SIZE;
        const auto inode = decode_inode(
            Span<const std::uint8_t>(inode_page.body.data() + offset, container::SVLT_INODE_SIZE));
        if (!inode.is_ok()) {
            return Result<ManifestState>::err(inode.error(), inode.message());
        }
        state.inodes.push_back(inode.value());
    }

    state.bitmap = bitmap_page.body;
    const auto hash = compute_manifest_root_hash(state);
    if (!hash.is_ok()) {
        return Result<ManifestState>::err(hash.error(), hash.message());
    }
    state.root_hash = hash.value();
    return Result<ManifestState>::ok(std::move(state));
}

Result<void> encrypt_manifest_page(
    crypto::CryptoProvider& crypto,
    const Span<const std::uint8_t> vmk,
    const ManifestPage& page,
    const Span<std::uint8_t> ciphertext_out,
    const Span<std::uint8_t> tag_out) noexcept {
    if (kManifestPageHeaderSize + page.body.size() > kManifestMaxPagePlaintext) {
        return Result<void>::err(ErrorCode::ManifestFull, "manifest page plaintext too large");
    }
    if (ciphertext_out.size() != kManifestMaxPagePlaintext) {
        return Result<void>::err(ErrorCode::InvalidArgument, "manifest ciphertext size mismatch");
    }
    if (tag_out.size() != crypto::kAeadTagSize) {
        return Result<void>::err(ErrorCode::InvalidArgument, "manifest tag size mismatch");
    }

    std::vector<std::uint8_t> plaintext(kManifestMaxPagePlaintext, 0);
    const auto hdr = encode_manifest_page_header(page.header, Span<std::uint8_t>(plaintext.data(), kManifestPageHeaderSize));
    if (!hdr.is_ok()) {
        return hdr;
    }
    if (!page.body.empty()) {
        std::memcpy(plaintext.data() + kManifestPageHeaderSize, page.body.data(), page.body.size());
    }

    const auto key = crypto::hkdf_manifest_page_key(vmk, page.header.generation, page.header.page_index);
    if (!key.is_ok()) {
        return Result<void>::err(key.error(), key.message());
    }
    crypto::AeadNonce nonce {};
    const auto derived_nonce = derive_manifest_nonce(page.header.generation, page.header.page_index);
    std::memcpy(nonce.bytes.data(), derived_nonce.data(), derived_nonce.size());

    return crypto.aead_encrypt(plaintext, Span<const std::uint8_t>{}, key.value(), nonce, ciphertext_out, tag_out);
}

Result<ManifestPage> decrypt_manifest_page(
    crypto::CryptoProvider& crypto,
    const Span<const std::uint8_t> vmk,
    const Span<const std::uint8_t> ciphertext,
    const Span<const std::uint8_t> tag,
    const std::uint64_t generation,
    const std::uint64_t page_index) noexcept {
    if (ciphertext.size() != kManifestMaxPagePlaintext) {
        return Result<ManifestPage>::err(ErrorCode::InvalidArgument, "manifest ciphertext size mismatch");
    }
    if (tag.size() != crypto::kAeadTagSize) {
        return Result<ManifestPage>::err(ErrorCode::InvalidArgument, "manifest tag size mismatch");
    }

    std::vector<std::uint8_t> plaintext(ciphertext.size());
    const auto key = crypto::hkdf_manifest_page_key(vmk, generation, page_index);
    if (!key.is_ok()) {
        return Result<ManifestPage>::err(key.error(), key.message());
    }
    crypto::AeadNonce nonce {};
    const auto derived_nonce = derive_manifest_nonce(generation, page_index);
    std::memcpy(nonce.bytes.data(), derived_nonce.data(), derived_nonce.size());
    const auto dec = crypto.aead_decrypt(ciphertext, Span<const std::uint8_t>{}, key.value(), nonce, tag, plaintext);
    if (!dec.is_ok()) {
        return Result<ManifestPage>::err(ErrorCode::ManifestDecryptFailed, dec.message());
    }

    if (plaintext.size() < kManifestPageHeaderSize) {
        return Result<ManifestPage>::err(ErrorCode::CorruptContainer, "manifest plaintext too short");
    }

    const auto header = decode_manifest_page_header(Span<const std::uint8_t>(plaintext.data(), kManifestPageHeaderSize));
    if (!header.is_ok()) {
        return Result<ManifestPage>::err(header.error(), header.message());
    }

    ManifestPage page {};
    page.header = header.value();
    const std::size_t available_body = plaintext.size() - kManifestPageHeaderSize;
    std::size_t body_size = 0;
    switch (page.header.page_type) {
    case container::ManifestPageType::InodeTable:
        body_size = static_cast<std::size_t>(page.header.entry_count) * container::SVLT_INODE_SIZE;
        break;
    case container::ManifestPageType::FreeBitmap:
        body_size = static_cast<std::size_t>(page.header.entry_count);
        break;
    default:
        return Result<ManifestPage>::err(ErrorCode::CorruptContainer, "unknown manifest page type");
    }
    if (body_size > available_body) {
        return Result<ManifestPage>::err(ErrorCode::CorruptContainer, "manifest page body too short");
    }
    page.body.assign(
        plaintext.begin() + static_cast<std::ptrdiff_t>(kManifestPageHeaderSize),
        plaintext.begin() + static_cast<std::ptrdiff_t>(kManifestPageHeaderSize + body_size));
    return Result<ManifestPage>::ok(std::move(page));
}

} 