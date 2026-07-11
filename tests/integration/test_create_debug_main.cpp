#include <sv/crypto/kdf_engine.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/manifest.hpp>
#include <sv/vault/vault_engine.hpp>

#include <chrono>
#include <iostream>
#include <sodium.h>

int main() {
    std::cout << "step: init\n" << std::flush;
    if (!sv::crypto::KdfEngine::is_available()) {
        std::cout << "FAIL: kdf unavailable\n";
        return 1;
    }

    sv::vault::Inode root {};
    root.entry_type = sv::container::InodeEntryType::Directory;
    root.inode_id = 1;

    sv::vault::ManifestState state {};
    state.generation = 1;
    state.inodes.push_back(root);
    state.bitmap.assign(1, 0);

    std::cout << "step: compute_manifest_root_hash\n" << std::flush;
    const auto hash = sv::vault::compute_manifest_root_hash(state);
    std::cout << "step: hash returned ok=" << hash.is_ok() << "\n" << std::flush;
    if (!hash.is_ok()) {
        std::cout << "FAIL hash: " << hash.message() << "\n";
        return 1;
    }
    std::cout << "step: manifest ok\n" << std::flush;

    const auto path = std::filesystem::path("C:/Temp/sv_debug_create.svlt");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = "integration-test-password-12";

    sv::vault::CreateVaultOptions opts {};
    opts.container_size = 4ULL * 1024 * 1024;
    opts.chunk_size = 64 * 1024;
    opts.kdf_params.m_cost_kib = 4096;
    opts.kdf_params.t_cost = 1;
    opts.kdf_params.p_cost = 1;

    sv::vault::VaultEngine engine;
    const auto start = std::chrono::steady_clock::now();
    std::cout << "step: create_new\n" << std::flush;
    const auto result = engine.create_new(path, cred, opts);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    if (!result.is_ok()) {
        std::cout << "FAIL code=" << static_cast<int>(result.error()) << " msg=" << result.message()
                  << " (" << ms.count() << "ms)\n";
        return 1;
    }

    std::cout << "OK create_new (" << ms.count() << "ms)\n" << std::flush;

    sv::vault::OpenVaultOptions open_opts {};
    open_opts.kdf_params = opts.kdf_params;
    auto session = engine.open(path, cred, open_opts);
    if (!session.is_ok()) {
        std::cout << "FAIL open code=" << static_cast<int>(session.error())
                  << " msg=" << session.message() << "\n";
        std::filesystem::remove(path, ec);
        return 1;
    }
    std::cout << "OK open\n" << std::flush;

#if !defined(NDEBUG)
    std::cout << "SKIP put/get in Debug (MSVC debug heap + manifest buffers); run Release sv_create_debug or "
                 "[integration] tests for end-to-end I/O\n"
              << std::flush;
    std::filesystem::remove(path, ec);
    return 0;
#endif

    const std::string payload = "hello shadow vault";
    std::cout << "step: put\n" << std::flush;
    const auto put_start = std::chrono::steady_clock::now();
    const auto put = engine.put(
        session.value(),
        "/hello.txt",
        sv::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()));
    if (!put.is_ok()) {
        std::cout << "FAIL put code=" << static_cast<int>(put.error()) << " msg=" << put.message() << "\n";
        std::filesystem::remove(path, ec);
        return 1;
    }
    const auto put_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - put_start);
    std::cout << "OK put (" << put_ms.count() << "ms)\n" << std::flush;

    const auto got = engine.get(session.value(), "/hello.txt");
    if (!got.is_ok()) {
        std::cout << "FAIL get code=" << static_cast<int>(got.error()) << " msg=" << got.message() << "\n";
        std::filesystem::remove(path, ec);
        return 1;
    }
    std::cout << "OK get size=" << got.value().size() << "\n" << std::flush;

    std::filesystem::remove(path, ec);
    return 0;
}