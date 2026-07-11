#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/vault/inode.hpp>
#include <sv/vault/manifest.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace {

sv::vault::Inode make_dir(const std::uint64_t inode_id, const std::uint64_t parent_id, const std::string& name) {
    sv::vault::Inode inode {};
    inode.entry_type = sv::container::InodeEntryType::Directory;
    inode.inode_id = inode_id;
    inode.parent_id = parent_id;
    inode.name_utf8 = name;
    inode.mtime_unix = 1700000000 + inode_id;
    return inode;
}

} // namespace

TEST_CASE("M-01 empty manifest create", "[unit][vault][m01]") {
    const auto manifest = sv::vault::create_empty_manifest(8ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(manifest.is_ok());
    REQUIRE(manifest.value().generation == 1);
    REQUIRE(manifest.value().inodes.size() == 1);
    REQUIRE(manifest.value().inodes.front().entry_type == sv::container::InodeEntryType::Directory);
    REQUIRE(manifest.value().inodes.front().inode_id == 1);
    REQUIRE_FALSE(manifest.value().bitmap.empty());
    REQUIRE_FALSE(std::all_of(
        manifest.value().root_hash.begin(),
        manifest.value().root_hash.end(),
        [](const std::uint8_t b) { return b == 0; }));
}

TEST_CASE("M-02 directory tree serialize/deserialize", "[unit][vault][m02]") {
    auto manifest = sv::vault::create_empty_manifest(8ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(manifest.is_ok());

    manifest.value().inodes.push_back(make_dir(2, 1, "docs"));
    manifest.value().inodes.push_back(make_dir(3, 2, "projects"));
    std::sort(manifest.value().inodes.begin(), manifest.value().inodes.end(), sv::vault::inode_less);

    const auto pages = sv::vault::manifest_to_pages(manifest.value());
    REQUIRE(pages.is_ok());

    const auto restored = sv::vault::manifest_from_pages(pages.value().first, pages.value().second);
    REQUIRE(restored.is_ok());
    REQUIRE(restored.value().inodes.size() == 3);
    REQUIRE(sv::vault::find_inode(restored.value().inodes, 1, "docs") != restored.value().inodes.end());
    REQUIRE(sv::vault::find_inode(restored.value().inodes, 2, "projects") != restored.value().inodes.end());
}

TEST_CASE("M-03 manifest generation is preserved across pages", "[unit][vault][m03]") {
    auto manifest = sv::vault::create_empty_manifest(4ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(manifest.is_ok());
    manifest.value().generation = 42;

    const auto pages = sv::vault::manifest_to_pages(manifest.value());
    REQUIRE(pages.is_ok());
    REQUIRE(pages.value().first.header.generation == 42);
    REQUIRE(pages.value().second.header.generation == 42);

    const auto hash_before = sv::vault::compute_manifest_root_hash(manifest.value());
    manifest.value().generation = 43;
    const auto hash_after = sv::vault::compute_manifest_root_hash(manifest.value());
    REQUIRE(hash_before.is_ok());
    REQUIRE(hash_after.is_ok());
    REQUIRE(hash_before.value() != hash_after.value());
}

TEST_CASE("M-04 manifest AEAD tamper detection", "[unit][vault][m04]") {
    auto crypto = sv::crypto::create_default_crypto_provider();
    REQUIRE(crypto != nullptr);

    const auto manifest = sv::vault::create_empty_manifest(4ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(manifest.is_ok());
    const auto pages = sv::vault::manifest_to_pages(manifest.value());
    REQUIRE(pages.is_ok());

    std::array<std::uint8_t, 32> vmk {};
    vmk.fill(0x90);
    std::vector<std::uint8_t> ciphertext(sv::vault::kManifestMaxPagePlaintext);
    std::array<std::uint8_t, sv::crypto::kAeadTagSize> tag {};

    const auto enc = sv::vault::encrypt_manifest_page(
        *crypto,
        vmk,
        pages.value().first,
        sv::Span<std::uint8_t>(ciphertext.data(), ciphertext.size()),
        sv::Span<std::uint8_t>(tag.data(), tag.size()));
    REQUIRE(enc.is_ok());

    ciphertext[1024] ^= 0x01;
    const auto dec = sv::vault::decrypt_manifest_page(
        *crypto,
        vmk,
        sv::Span<const std::uint8_t>(ciphertext.data(), ciphertext.size()),
        sv::Span<const std::uint8_t>(tag.data(), tag.size()),
        manifest.value().generation,
        0);
    REQUIRE_FALSE(dec.is_ok());
}