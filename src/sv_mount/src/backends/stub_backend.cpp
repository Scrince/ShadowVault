#include <sv/mount/backends/stub_backend.hpp>

namespace sv::mount {

namespace {

constexpr const char* k_stub_message =
    "Mount backend not available: this build has no WinFsp/FUSE provider. "
    "On Windows install WinFsp and rebuild with -DSV_BUILD_MOUNT=ON. "
    "On Linux install libfuse and rebuild with -DSV_BUILD_MOUNT=ON. "
    "Until then use svcli put/get/list (no drive letter).";

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