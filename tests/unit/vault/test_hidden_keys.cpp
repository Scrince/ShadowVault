#include <sv/crypto/kdf_engine.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/hidden_volume.hpp>
#include <sv/vault/vault_engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace {

std::filesystem::path temp_vault_path() {
    static std::atomic<std::uint64_t> counter {0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    return std::filesystem::path("C:/Temp")
        / ("sv_unit_hv07_" + std::to_string(stamp) + "_" + std::to_string(id) + ".svlt");
}

sv::vault::CredentialMaterial outer_cred() {
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = "outer-unit-password-12";
    return cred;
}

sv::vault::CredentialMaterial hidden_cred() {
    sv::vault::CredentialMaterial cred {};
    cred.password_utf8 = "hidden-unit-password-9";
    return cred;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} 

TEST_CASE("HV-07 outer and hidden volumes use different VMK", "[unit][vault][hv07]") {
    REQUIRE(sv::crypto::KdfEngine::is_available());

    const auto path = temp_vault_path();
    sv::vault::VaultEngine engine;

    sv::vault::CreateVaultOptions opts {};
    opts.container_size = 128ULL * 1024 * 1024;
    opts.chunk_size = 64 * 1024;
    opts.kdf_params.m_cost_kib = 4096;
    opts.kdf_params.t_cost = 1;
    opts.kdf_params.p_cost = 1;

    REQUIRE(engine.create_new(path, outer_cred(), opts).is_ok());

    sv::vault::OpenVaultOptions outer_opts {};
    auto outer = engine.open(path, outer_cred(), outer_opts);
    REQUIRE(outer.is_ok());
    REQUIRE(sv::vault::create_hidden_volume(
                engine,
                outer.value(),
                hidden_cred(),
                32ULL * 1024 * 1024)
                .is_ok());

    std::array<std::uint8_t, 32> outer_vmk_bytes {};
    std::memcpy(outer_vmk_bytes.data(), outer.value().vmk.span().data(), outer_vmk_bytes.size());
    {
        sv::vault::UnlockedVault closed = std::move(outer.value());
        (void)closed;
    }

    sv::vault::OpenVaultOptions hidden_opts {};
    hidden_opts.intent = sv::vault::HeaderSlotIntent::Hidden;
    auto hidden = engine.open(path, hidden_cred(), hidden_opts);
    REQUIRE(hidden.is_ok());

    const auto hidden_vmk = hidden.value().vmk.span();
    REQUIRE(hidden_vmk.size() == 32);
    REQUIRE(std::memcmp(outer_vmk_bytes.data(), hidden_vmk.data(), 32) != 0);

    cleanup(path);
}