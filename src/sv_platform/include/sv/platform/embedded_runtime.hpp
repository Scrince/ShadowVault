#pragma once

namespace sv::platform {

/// Windows single-exe portable: extract embedded WinFsp DLL and default config on first run.
void bootstrap_embedded_runtime() noexcept;

} // namespace sv::platform