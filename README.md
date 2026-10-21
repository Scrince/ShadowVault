# Shadow Vault

Shadow Vault is an encrypted vault application for Windows-focused secure file storage. It provides a `.svlt` container format, a command-line tool, optional Qt GUI, hidden-volume support, portable USB/traveler mode, integrity verification, repair tooling, and an experimental mount backend.

The project is implemented in modern C++ with CMake and uses vendored cryptographic dependencies by default.

## Features

- Encrypted `.svlt` vault containers
- Password and keyfile based unlocking
- File operations: `put`, `get`, `list`, `mkdir`, `rename`, and `remove`
- Hidden-volume creation and hidden-volume protection
- Header backup/restore, verification, and repair flows
- Portable mode for removable media with reduced host traces
- Optional WinFsp mount support on Windows
- Optional Qt Widgets GUI
- Phase 1 pre-boot authentication (PBA) lab/prototype components
- Unit, integration, and fuzz test targets

## Project Status

Shadow Vault is currently at version `1.2.0`.

| Area | Status |
| --- | --- |
| Core container, headers, manifest, journal | Complete |
| Crypto primitives and KDF integration | Complete |
| CLI vault lifecycle and file operations | Complete |
| Verification, recovery, and repair | Complete |
| Hidden volumes | Complete |
| Keyfile support | Complete |
| Portable/traveler mode | Complete |
| Qt GUI | Optional build |
| WinFsp mount backend | Optional Windows build |
| System encryption | Design documents only |
| PBA prototype | Lab/QEMU prototype behind build flags |

## Requirements

- CMake 3.20 or newer
- C++20 compiler
  - Windows: Visual Studio 2022 / MSVC recommended
  - Linux/macOS: GCC 11+ or Clang 14+
- PowerShell 5.1+ for Windows packaging scripts
- Optional: Qt 6 for the GUI
- Optional: WinFsp for Windows mount support
- Optional: WiX 3.11+ for MSI installer packaging

## Quick Build

### Windows with Visual Studio

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSV_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The CLI binary is produced at:

```text
build\src\sv_cli\Release\svcli.exe
```

### Single-Configuration Generators

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## CMake Options

| Option | Default | Description |
| --- | --- | --- |
| `SV_BUILD_CLI` | `ON` | Build the `svcli` executable |
| `SV_BUILD_GUI` | `ON` | Build the Qt GUI (primary desktop experience); stub if Qt is unavailable |
| `SV_BUILD_MOUNT` | `OFF` | Build mount backends |
| `SV_BUILD_PBA` | `OFF` | Build Phase 1 PBA host/lab components |
| `SV_BUILD_PBA_EFI` | `OFF` | Build `ShadowVaultBoot.efi` through EDK2 |
| `SV_BUILD_TESTS` | `ON` | Build and register tests |
| `SV_BUILD_FUZZ` | `OFF` | Build fuzz targets |
| `SV_PACKAGE_INSTALLER` | `ON` | Enable install/package targets |
| `SV_SINGLE_EXE_PORTABLE` | `OFF` | Build a Windows single-file portable executable |
| `SV_USE_VENDORED_LIBSODIUM` | `ON` | Use vendored libsodium |
| `SV_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer where supported |
| `SV_ENABLE_LTO` | `OFF` | Enable link-time optimization for Release builds |

## Optional Builds

### GUI

Set `SV_BUILD_GUI=ON` and point CMake at a Qt 6 installation:

```powershell
cmake -S . -B build-gui -G "Visual Studio 17 2022" -A x64 `
  -DSV_BUILD_GUI=ON `
  -DSV_QT_ROOT_PATH=C:\Qt\6.9.0\msvc2022_64
cmake --build build-gui --config Release --target svgui
```

Output:

```text
build-gui\src\sv_gui\Release\svgui.exe
```

### Windows Mount Support

Install WinFsp, then configure with mount support:

```powershell
cmake -S . -B build-mount -G "Visual Studio 17 2022" -A x64 `
  -DSV_BUILD_MOUNT=ON `
  -DSV_BUILD_TESTS=ON
cmake --build build-mount --config Release
```

If needed, set `SV_WINFSP_RUNTIME_DLL` or `SV_WINFSP_ROOT` explicitly. The build copies `winfsp-x64.dll` beside the CLI/GUI binaries when it can find the runtime DLL.

### Single-File Portable Executable

```powershell
.\scripts\build-single-exe.ps1
```

Outputs are written to `dist\` by default:

```text
dist\ShadowVault.exe
dist\ShadowVault-1.2.0-portable.exe
```

The single-file executable is designed for USB/traveler use. On first run, it stages its portable configuration and runtime files beside or near the executable as needed.

### Portable Folder / ZIP

```powershell
.\scripts\build-portable.ps1 -BuildDir C:\Temp\shadowvault-build -OutputDir C:\Temp\shadowvault-dist
```

This creates a USB-ready folder and ZIP containing `svcli.exe`, portable configuration, launch scripts, docs, and working directories.

### Windows Installer

```powershell
.\scripts\build-installer.ps1 -BuildDir C:\Temp\shadowvault-build -IncludePba
```

Expected outputs:

```text
C:\Temp\shadowvault-dist\ShadowVault-1.2.0-win64.msi
C:\Temp\shadowvault-dist\ShadowVault-1.2.0-win64-portable.zip
```

Install the MSI from an elevated shell:

```powershell
msiexec /i C:\Temp\shadowvault-dist\ShadowVault-1.2.0-win64.msi
```

## CLI Usage

Run:

```powershell
.\build\src\sv_cli\Release\svcli.exe --help
```

Typical workflow:

```powershell
.\svcli.exe create vault.svlt --size 64M --password "change-this-passphrase" --quick
.\svcli.exe --vault vault.svlt mkdir /docs --password "change-this-passphrase"
.\svcli.exe --vault vault.svlt put .\local-file.txt /docs/local-file.txt --password "change-this-passphrase"
.\svcli.exe --vault vault.svlt list / --password "change-this-passphrase"
.\svcli.exe --vault vault.svlt get /docs/local-file.txt .\restored-file.txt --password "change-this-passphrase"
.\svcli.exe verify vault.svlt --deep --password "change-this-passphrase"
.\svcli.exe lock vault.svlt
```

`--vault` is a global option, so place it before the command name. For real vaults, prefer interactive password prompts or a protected password file instead of putting secrets directly in shell history.

Hidden-volume and recovery examples:

```powershell
.\svcli.exe create-hidden vault.svlt --size 32M
.\svcli.exe backup-header vault.svlt --output vault-header.svhdr
.\svcli.exe restore-header vault.svlt --input vault-header.svhdr
.\svcli.exe repair vault.svlt --from-backup
```

Mount support, when built with `SV_BUILD_MOUNT=ON`:

```powershell
.\svcli.exe mount vault.svlt --point Y: --password "change-this-passphrase" --allow-host-integration
```

The mount command keeps running while the vault is mounted. Press `Ctrl+C` to dismount cleanly.

For the full command reference, see [docs/cli-spec.md](docs/cli-spec.md).

## Tests

Build with `SV_BUILD_TESTS=ON`, then run:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Direct test binaries are also produced under the build tree:

```text
build\tests\Release\sv_unit_tests.exe
build\tests\Release\sv_integration_tests.exe
```

Some integration tests require optional runtime support. For example, mount tests require a WinFsp-enabled build and an available drive letter.

## Repository Layout

```text
cmake/          CMake helpers, package rules, version generation
docs/           Architecture, specifications, threat models, and plans
external/       Vendored third-party source and dependency metadata
packaging/      Windows installer, portable layout, completion files
scripts/        Build, package, signing, and PBA lab scripts
src/            Shadow Vault libraries, CLI, GUI, mount, and PBA code
tests/          Unit tests, integration tests, fuzz targets, vectors
dist/           Local release outputs, when present
```

## Documentation

Important design and specification documents:

- [Architecture](docs/architecture.md)
- [CLI specification](docs/cli-spec.md)
- [File format specification](docs/file-format-spec.md)
- [Key hierarchy](docs/key-hierarchy.md)
- [Hidden volume design](docs/hidden-volume-design.md)
- [Portable mode design](docs/portable-mode-design.md)
- [Mount backend design](docs/mount-backend-design.md)
- [Threat model](docs/threat-model.md)
- [System encryption RFC](docs/rfc-system-encryption.md)
- [PBA implementation plan](docs/phase1-pba-implementation-plan.md)
- [Test plan](docs/test-plan.md)

## Security Notes

Shadow Vault is security-sensitive software. Treat local builds, test vectors, and generated vaults accordingly.

- Keep backups of vaults and headers before testing recovery flows.
- Do not rely on command-line passwords in shared shell history.
- Prefer interactive password prompts for real vaults.
- Verify release artifacts and signatures before distributing binaries.
- Review the threat model before using experimental features.

## License

Shadow Vault is licensed under GPLv3. See [LICENSE](LICENSE), [docs/DECISIONS.md](docs/DECISIONS.md), and [docs/dependency-policy.md](docs/dependency-policy.md) for licensing and dependency notes.
