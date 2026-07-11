#include "test_fixtures.hpp"

#include <sv/container/container_file.hpp>

#include <cstdio>
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

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <output_dir>\n", argv[0]);
        return 2;
    }

    if (!sv::crypto::KdfEngine::is_available()) {
        std::fprintf(stderr, "crypto unavailable\n");
        return 3;
    }

    const auto out_dir = std::filesystem::path(argv[1]);
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);

    const auto vault_path = out_dir / "lab.vlt";
    sv::vault::VaultEngine engine;

    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = "shadowvault-lab";

    const auto opts = sv::test::pba_lab_create_options();
    const auto created = engine.create_new(vault_path, cred, opts);
    if (!created.is_ok()) {
        std::fprintf(stderr, "create_new failed: %s\n", created.message().c_str());
        return 4;
    }

    auto container = sv::container::ContainerFileStore::open(vault_path);
    if (!container.is_ok()) {
        std::fprintf(stderr, "open failed: %s\n", container.message().c_str());
        return 5;
    }

    const auto tail_sb = container.value().read_superblock_tail();
    if (!tail_sb.is_ok()) {
        std::fprintf(stderr, "tail superblock failed: %s\n", tail_sb.message().c_str());
        return 6;
    }

    const auto header = container.value().read_region(
        tail_sb.value().primary_header_offset,
        sv::container::SVLT_HEADER_SIZE);
    if (!header.is_ok()) {
        std::fprintf(stderr, "read header failed: %s\n", header.message().c_str());
        return 7;
    }

    const auto superblock_offset = tail_sb.value().container_size - sv::container::SVLT_SUPERBLOCK_SIZE;
    const auto superblock = container.value().read_region(
        superblock_offset,
        sv::container::SVLT_SUPERBLOCK_SIZE);
    if (!superblock.is_ok()) {
        std::fprintf(stderr, "read superblock failed: %s\n", superblock.message().c_str());
        return 8;
    }

    const auto lab_dir = out_dir / "lab";
    std::filesystem::create_directories(lab_dir, ec);

    if (!write_file(lab_dir / "header.bin", header.value())
        || !write_file(lab_dir / "superblock.bin", superblock.value())) {
        std::fprintf(stderr, "write lab vectors failed\n");
        return 9;
    }

    std::printf("lab vectors: %s\n", lab_dir.string().c_str());
    std::printf("passphrase: shadowvault-lab\n");
    return 0;
}