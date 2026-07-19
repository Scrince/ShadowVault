#include <sv/vault/manifest.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

sv::vault::ManifestState build_golden_manifest() {
    const auto created = sv::vault::create_empty_manifest(4ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(created.is_ok());
    return created.value();
}

} 

TEST_CASE("M-06 manifest_root_hash golden vector", "[unit][vault][m06]") {
    const auto manifest = build_golden_manifest();
    const auto hash = sv::vault::compute_manifest_root_hash(manifest);
    REQUIRE(hash.is_ok());
    REQUIRE(manifest.root_hash == hash.value());

    const auto again = sv::vault::create_empty_manifest(4ULL * 1024 * 1024, 64 * 1024);
    REQUIRE(again.is_ok());
    REQUIRE(again.value().root_hash == hash.value());
}

TEST_CASE("M-06 manifest_root_hash stable across page roundtrip", "[unit][vault][m06]") {
    const auto manifest = build_golden_manifest();
    const auto pages = sv::vault::manifest_to_pages(manifest);
    REQUIRE(pages.is_ok());

    const auto restored = sv::vault::manifest_from_pages(pages.value().first, pages.value().second);
    REQUIRE(restored.is_ok());
    REQUIRE(restored.value().root_hash == manifest.root_hash);
}