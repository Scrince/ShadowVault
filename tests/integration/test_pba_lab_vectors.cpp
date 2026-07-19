#include "test_fixtures.hpp"

#include <sv/container/container_file.hpp>
#include <sv/pba/pba_unlock.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

bool write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

} 

TEST_CASE("P1-07 exported lab vectors unlock on host", "[integration][pba][p107]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto base = sv::test::temp_vault_path("sv_pba_lab_export");
    const auto lab_dir = base.parent_path() / (base.stem().string() + "_lab");
    std::error_code ec;
    std::filesystem::remove_all(lab_dir, ec);

    const auto vault_path = lab_dir / "lab.vlt";
    std::filesystem::create_directories(lab_dir, ec);

    sv::vault::VaultEngine engine;
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = "shadowvault-lab";
    const auto opts = sv::test::pba_lab_create_options();

    REQUIRE(engine.create_new(vault_path, cred, opts).is_ok());

    std::array<std::uint8_t, 32> expected_vmk {};
    {
        sv::vault::OpenVaultOptions open_opts {};
        auto session = engine.open(vault_path, cred, open_opts);
        REQUIRE(session.is_ok());
        REQUIRE(session.value().vmk.size() == expected_vmk.size());
        std::memcpy(expected_vmk.data(), session.value().vmk.data(), expected_vmk.size());
        sv::test::release_session(session.value());
    }

    auto container = sv::container::ContainerFileStore::open(vault_path);
    REQUIRE(container.is_ok());

    const auto tail_sb = container.value().read_superblock_tail();
    REQUIRE(tail_sb.is_ok());

    const auto header = container.value().read_region(
        tail_sb.value().primary_header_offset,
        sv::container::SVLT_HEADER_SIZE);
    REQUIRE(header.is_ok());

    const auto superblock_offset = tail_sb.value().container_size - sv::container::SVLT_SUPERBLOCK_SIZE;
    const auto superblock = container.value().read_region(
        superblock_offset,
        sv::container::SVLT_SUPERBLOCK_SIZE);
    REQUIRE(superblock.is_ok());

    REQUIRE(write_file(lab_dir / "header.bin", header.value()));
    REQUIRE(write_file(lab_dir / "superblock.bin", superblock.value()));

    const auto header_bytes = read_file(lab_dir / "header.bin");
    const auto superblock_bytes = read_file(lab_dir / "superblock.bin");

    const char* password = "shadowvault-lab";
    sv::pba::PbaUnlockRequest request {};
    request.primary_header_sector = sv::Span<const std::uint8_t>(header_bytes.data(), header_bytes.size());
    request.superblock_bytes = sv::Span<const std::uint8_t>(superblock_bytes.data(), superblock_bytes.size());
    request.credentials.password_utf8 = sv::Span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(password),
        std::strlen(password));

    const auto unlocked = sv::pba::unlock_primary_header(request);
    REQUIRE(unlocked.is_ok());
    REQUIRE(unlocked.value() == expected_vmk);

    std::filesystem::remove_all(lab_dir, ec);
}