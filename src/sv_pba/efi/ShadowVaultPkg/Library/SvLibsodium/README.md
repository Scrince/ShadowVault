# SvLibsodium — UEFI libsodium port (Sprint B+)

`sv_pba_freestanding` needs these libsodium APIs:

- `sodium_init`
- `crypto_hash_sha256`
- `crypto_pwhash_argon2id`
- `crypto_aead_xchacha20poly1305_ietf_decrypt_detached`
- `crypto_auth_hmacsha256`
- `sodium_memcmp` / `sodium_memzero`

## Source tree

Fetch with:

```powershell
T:\ShadowVault\scripts\pba-fetch-libsodium.ps1
```

Set for LabUnlock builds:

```powershell
$env:LIBSODIUM_ROOT = 'C:\Temp\libsodium-src'
$env:LIBSODIUM_INCLUDE = "$env:LIBSODIUM_ROOT\src\libsodium\include"
```

## UEFI allocator

`sodium_alloc_uefi.c` routes libsodium heap calls to `AllocatePool` / `FreePool`.

## EDK2 module

`SvLibsodium.inf` compiles the minimal libsodium source set (matching
`external/libsodium_cmake/CMakeLists.txt`) plus UEFI platform shims:

| Shim | Role |
|------|------|
| `sodium_platform_uefi.c` | `malloc` / `free` / `calloc` → `AllocatePool` |
| `sodium_crt_uefi.c` | `memcpy`, `memset`, `strlen`, `strchr`, `errno` |
| `sodium_utils_minimal.c` | `sodium_memzero`, `sodium_memcmp` |
| `randombytes_uefi_api.c` | Stub RNG (unlock path is decrypt-only) |
| `sodium_core_patch.c` | `abort` → `CpuDeadLoop` |

Sources live under `external/libsodium-src` (fetch via `scripts/pba-fetch-libsodium.ps1`).

## Build

```powershell
.\build-edk2.ps1 -Edk2Root C:\edk2 -Mode LabUnlock
```

No external `sodium.lib` required.