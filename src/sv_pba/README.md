# Shadow Vault — Pre-Boot Authentication (Phase 1)

Freestanding PBA core and EFI bootloader prototype for system encryption.

**Status:** Sprint A–D complete for lab — freestanding unlock, EFI UI, chain-load, rescue stub.

See `docs/phase1-pba-implementation-plan.md` and `docs/rfc-system-encryption.md`.

## Build

```bash
cmake -B build -DSV_BUILD_PBA=ON -DSV_BUILD_TESTS=ON
cmake --build build --target sv_unit_tests
ctest -R pba
```

Host unlock uses `sv_pba_freestanding` (C, libsodium) via `sv_pba_unlock_primary_header()` — same path EFI will link.

`SV_BUILD_PBA` defaults to **OFF**. No EFI toolchain is required for host-side `sv_pba_core` tests.

### EFI (optional)

```powershell
cmake -B build -DSV_BUILD_PBA=ON -DSV_BUILD_PBA_EFI=ON -DEDK2_ROOT=C:/edk2
cmake --build build --target ShadowVaultBoot_efi
```

See `efi/README.md` and `scripts/pba-qemu.ps1`.