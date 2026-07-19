#include <sv/mount/backends/stub_backend.hpp>

namespace sv::mount {

namespace {

constexpr const char* k_stub_message =
    "Install WinFsp (Windows) or FUSE (Linux) for mount support";

} 

std::string StubBackend::name() const {
    return "stub";
}

Result<void> StubBackend::mount(
    const std::filesystem::path& ,
    const MountOptions& ) {
    return Result<void>::err(ErrorCode::NotImplemented, k_stub_message);
}

Result<void> StubBackend::dismount(const std::filesystem::path& ) {
    return Result<void>::err(ErrorCode::NotImplemented, k_stub_message);
}

std::vector<MountSession> StubBackend::list_active() const {
    return {};
}

} 