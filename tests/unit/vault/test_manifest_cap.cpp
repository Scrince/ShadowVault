#include <sv/container/types.hpp>
#include <sv/vault/inode.hpp>
#include <sv/vault/manifest.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

TEST_CASE("M-05 rejects inode table beyond v1 capacity", "[unit][vault][m05]") {
    auto manifest = sv::vault::create_empty_manifest(4ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(manifest.is_ok());

    for (std::uint64_t inode_id = 2; inode_id <= sv::container::SVLT_MAX_INODES_V1; ++inode_id) {
        sv::vault::Inode file {};
        file.entry_type = sv::container::InodeEntryType::File;
        file.inode_id = inode_id;
        file.parent_id = 1;
        file.name_utf8 = "f" + std::to_string(inode_id);
        manifest.value().inodes.push_back(file);
    }
    std::sort(manifest.value().inodes.begin(), manifest.value().inodes.end(), sv::vault::inode_less);

    const auto at_limit = sv::vault::manifest_to_pages(manifest.value());
    REQUIRE(at_limit.is_ok());
    REQUIRE(at_limit.value().first.header.entry_count == sv::container::SVLT_MAX_INODES_V1);

    sv::vault::Inode overflow {};
    overflow.entry_type = sv::container::InodeEntryType::File;
    overflow.inode_id = sv::container::SVLT_MAX_INODES_V1 + 1;
    overflow.parent_id = 1;
    overflow.name_utf8 = "overflow";
    manifest.value().inodes.push_back(overflow);
    std::sort(manifest.value().inodes.begin(), manifest.value().inodes.end(), sv::vault::inode_less);

    const auto over_limit = sv::vault::manifest_to_pages(manifest.value());
    REQUIRE_FALSE(over_limit.is_ok());
    REQUIRE(over_limit.error() == sv::ErrorCode::ManifestFull);
}