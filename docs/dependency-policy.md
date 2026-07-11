# Shadow Vault — Dependency Policy

| Field | Value |
|-------|-------|
| **Date** | 2026-07-05 |
| **Status** | Draft |

---

## Overview

Shadow Vault minimizes external dependencies to reduce supply-chain risk, enable security audit, and avoid unwanted network/telemetry behavior. All third-party code must be justified, vendored with version pins, and documented.

---

## Principles

| # | Principle |
|---|-----------|
| P1 | Prefer from-scratch implementation for non-crypto utilities |
| P2 | Crypto: use small, widely audited libraries — vendored, not dynamic OS crypto alone |
| P3 | No runtime network access in core product |
| P4 | No telemetry, crash reporting to cloud, or auto-update |
| P5 | No GPL-incompatible static linking surprises without legal review |
| P6 | Pin versions; record SHA256 of vendored tarballs |
| P7 | Every dependency listed in `THIRD_PARTY_NOTICES.txt` |

---

## Approved Dependencies (v1)

| Library | Version Target | Purpose | License | Justification |
|---------|----------------|---------|---------|---------------|
| **libsodium** | 1.0.19+ | XChaCha20-Poly1305, RNG, secure zero, HKDF | ISC | Modern AEAD; widely audited; minimal API |
| **argon2** (PHC ref) | 20190702 | Argon2id KDF | Apache 2.0 / CC0 dual | RFC 9106 reference; tunable memory cost |
| **Catch2** or **doctest** | 3.x | Unit tests | BSL-1.0 | Header-friendly test framework (build-only) |
| **Qt 6** | 6.5+ LTS | GUI only (M8) | LGPL v3 | Cross-platform GUI; optional component |

### Explicitly Rejected (v1)

| Library | Reason |
|---------|--------|
| OpenSSL | Large attack surface; version fragmentation |
| Boost | Heavy; prefer std C++20 |
| zlib | Not needed v1 (no compression in container) |
| curl / HTTP libs | No network policy |
| Sentry / Crashpad cloud | Telemetry policy |
| SQLite | Manifest custom per architecture doc |

---

## Optional Dependencies (Later Milestones)

| Library | Milestone | Purpose | License |
|---------|-----------|---------|---------|
| WinFsp | M9 | Windows mount | GPLv3 + special |
| Dokan 2 | M9 fallback | Windows mount | LGPL/MIT |
| libfuse3 | M9 | Linux mount | LGPL |
| macFUSE | M9 | macOS mount | BSD-style |
| libFuzzer | Dev | Fuzzing | Apache 2.0 / LLVM |

---

## CryptoProvider Abstraction

**v1 KDF:** Argon2id only (`kdf_type=1`). scrypt/PBKDF2 deferred to v1.1.

All cryptographic operations go through `sv::crypto::CryptoProvider`:

- Enables known-answer tests with mock provider
- Allows future FIPS backend without rewriting vault logic
- **Default:** `LibsodiumCryptoProvider` wrapping vendored libsodium

**Not allowed:** Direct `#include <sodium.h>` outside `src/sv_crypto/`.

---

## Vendoring Strategy

```
external/
├── libsodium/          # Submodule tag 1.0.19-RELEASE
├── argon2/             # Vendor PHC release tarball
└── checksums.txt       # SHA256 pins
```

### Update Process

1. Security advisory monitoring (manual RSS/GitHub watch)
2. Proposed bump PR with:
   - Changelog diff
   - Re-run full `ctest` + crypto vectors
   - Update `THIRD_PARTY_NOTICES.txt`
3. Two maintainer review for crypto deps

---

## System / OS APIs (Not "Dependencies")

Permitted without vendoring:

| API | Use |
|-----|-----|
| Win32 File I/O | `sv_platform` |
| POSIX `open/read/write` | `sv_platform` |
| `BCryptGenRandom` / `/dev/urandom` | Seed CSPRNG |
| `mlock` / `VirtualLock` | Secure memory |

**Policy:** Do not delegate core crypto to OS CAPI/CNG alone — libsodium remains primary.

---

## Telemetry & Network

| Feature | Status |
|---------|--------|
| Phone home | **Forbidden** |
| Auto-update | **Forbidden** |
| Online activation | **Forbidden** |
| Crash upload | **Forbidden** |
| Update check URL | **Forbidden** |

Build-time CMake must not fetch remote resources without `SV_ALLOW_DOWNLOADS=ON` explicit dev flag.

---

## Licensing Compliance

| Component | Distribution |
|-----------|--------------|
| Shadow Vault core | GPLv3 (proposed) |
| libsodium | ISC notice in installer |
| argon2 | Apache 2.0 notice |
| Qt GUI | LGPL — provide source offer / dynamic link |

**File:** `packaging/THIRD_PARTY_NOTICES.txt`

---

## Audit Checklist

- [ ] No `system("curl")` or equivalent
- [ ] No Winsock / BSD sockets in `src/`
- [ ] `grep -r "http://" src/` empty except docs
- [ ] Submodules match `checksums.txt`
- [ ] Fuzz + unit tests pass on dep bump

---

## Minimal stdlib Usage

Prefer C++20 STL: `std::span`, `std::filesystem`, `std::optional`, `std::variant`.

No exceptions across crypto boundary if `-fno-exceptions` port needed — use `Result<T>` (design allows exceptions internally initially).

---

## Development-Only Tools

| Tool | Scope |
|------|-------|
| CMake | Build |
| Ninja | Build |
| clang-format | Dev |
| cppcheck | CI optional |

Not shipped in release binaries.

---

*Self-contained dependency policy for Shadow Vault.*