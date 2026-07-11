# ShadowVaultBoot.efi (Phase 1 — lab prototype)

UEFI pre-boot loader for Phase 1 PBA. Host-side crypto unlock is validated via `sv_pba_core`; this tree builds the firmware entry point, chain-loads `bootmgfw.efi`, and includes the QEMU lab harness.

## Layout

```
efi/
  ShadowVaultPkg/
    Application/ShadowVaultBoot/   # UefiMain + passphrase prompt
    Library/SvPbaEfiStub/          # Lab stub until freestanding core links
    Include/sv_pba_efi.h         # EFI ↔ core bridge API
  build-edk2.ps1 / build-edk2.sh
```

## Prerequisites

| Tool | Purpose | Setup script |
|------|---------|--------------|
| [EDK2](https://github.com/tianocore/edk2) | Build `ShadowVaultBoot.efi` | `scripts/pba-setup-edk2.ps1` |
| libsodium source | LabUnlock headers | `scripts/pba-fetch-libsodium.ps1` |
| VS2022 Build Tools (Windows) or GCC5 (Linux) | EDK2 toolchain (`-t VS2022` / `-t GCC5`) | — |
| QEMU + OVMF | Boot test (`scripts/pba-qemu.*`) | Install QEMU manually |
| Hyper-V (Windows) / mtools (Linux) | Create FAT ESP image | — |

Host-only verification (no EDK2/QEMU):

```powershell
.\scripts\pba-verify-lab.ps1
```

`PACKAGES_PATH` must include this `efi/` directory so EDK2 resolves `ShadowVaultPkg/`.

## Build (EDK2)

### Windows (VS2022)

```powershell
git clone https://github.com/tianocore/edk2.git C:\edk2
cd C:\edk2
git submodule update --init
# Run EDK2 BaseTools build once per edksetup docs

$env:EDK2_ROOT = 'C:\edk2'
T:\ShadowVault\src\sv_pba\efi\build-edk2.ps1 -Edk2Root $env:EDK2_ROOT
```

Output: `C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi`

### Linux (GCC5)

```bash
export EDK2_ROOT=~/edk2
T:/ShadowVault/src/sv_pba/efi/build-edk2.sh
```

## QEMU lab loop

LabUnlock accepts passphrase **`shadowvault-lab`** against ESP vectors under `EFI/ShadowVault/lab/`.

### Windows

```powershell
# Automated smoke (no admin — uses QEMU fat: drive)
.\scripts\pba-qemu-smoke.ps1

# Interactive boot (ESP directory)
.\scripts\pba-prepare-esp-dir.ps1 -EfiPath C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi -IncludeLabVectors
.\scripts\pba-qemu.ps1 -EspDir C:\Temp\shadowvault-pba-esp
# Passphrase: shadowvault-lab

# Automated QEMU tests (LabUnlock build)
.\scripts\pba-qemu-smoke.ps1 -TestChainLoad
.\scripts\pba-qemu-smoke.ps1 -TestWrongPassword
.\scripts\pba-qemu-smoke.ps1 -TestRescue

# Physical USB lab (Administrator)
.\scripts\pba-physical-lab.ps1 -UsbDrive E: -EfiPath C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi -IncludeLabVectors

# Optional: VHD ESP image (Administrator)
.\scripts\pba-make-esp.ps1 -EfiPath C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi -IncludeLabVectors
.\scripts\pba-qemu.ps1 -EspImage C:\Temp\shadowvault-pba-esp.img
```

### Linux

```bash
chmod +x scripts/pba-*.sh src/sv_pba/efi/build-edk2.sh
./scripts/pba-make-esp.sh ~/edk2/Build/ShadowVaultPkg/RELEASE_GCC5/X64/ShadowVaultBoot.efi
./scripts/pba-qemu.sh
```

## CMake gate

`SV_BUILD_PBA_EFI=ON` registers a custom target that runs `build-edk2.*` when `EDK2_ROOT` is set. Default is **OFF** — firmware builds stay opt-in.

## Build modes

| Mode | DSC | Unlock |
|------|-----|--------|
| **Stub** (default) | `ShadowVaultPkg.dsc` | Passphrase `shadowvault-lab` accepted without crypto |
| **LabUnlock** | `ShadowVaultPkgFreestanding.dsc` | Reads `EFI/ShadowVault/lab/*.bin`, calls `sv_pba_unlock_primary_header` |

LabUnlock uses vendored **SvLibsodium** (no external `sodium.lib`). Fetch sources once:

```powershell
.\scripts\pba-fetch-libsodium.ps1 -Destination T:\ShadowVault\external\libsodium-src
.\build-edk2.ps1 -Edk2Root C:\edk2 -Mode LabUnlock
```

Output: `C:\edk2\Build\ShadowVaultPkg\RELEASE_VS2022\X64\ShadowVaultBoot.efi` (~98 KiB with Argon2id + XChaCha).

## Lab vectors (host export)

```powershell
cmake --build C:\Temp\shadowvault-build --config Release --target sv_pba_export_lab
.\scripts\pba-export-lab-vectors.ps1
.\scripts\pba-make-esp.ps1 -EfiPath ...\ShadowVaultBoot.efi -IncludeLabVectors
```

Passphrase: **`shadowvault-lab`**

## Sprint roadmap

| Sprint | EFI work |
|--------|----------|
| **B** | Passphrase UI, stub unlock, EDK2 + QEMU docs |
| **B+** | SvLibsodium UEFI port + LabUnlock (`sv_pba_unlock_primary_header` in firmware) |
| **C (current)** | Chain-load `bootmgfw.efi` / `BOOTX64.EFI` after unlock (`SvPbaChainLoad`) |
| **D** | Rescue USB detect stub; passphrase wipe before chain-load |

See `docs/phase1-pba-implementation-plan.md`.