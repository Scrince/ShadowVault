# Shadow Vault — Test Plan

| Field | Value |
|-------|-------|
| **Date** | 2026-07-05 |
| **Status** | Draft |
| **Framework** | Catch2 or doctest |

---

## Overview

This plan enumerates **mandatory** tests from project requirements plus additional regression coverage. Tests live under `T:\ShadowVault\tests\`.

---

## Test Layout

```
tests/
├── unit/
│   ├── crypto/
│   ├── container/
│   ├── vault/
│   ├── keyfile/
│   ├── recovery/
│   └── portable/
├── integration/
│   ├── vault_lifecycle.cpp
│   ├── file_ops.cpp
│   └── hidden_volume.cpp
├── vectors/
│   ├── kdf_argon2id_v1.json
│   ├── aead_xchacha_v1.json
│   └── header_golden_v1.bin
└── fuzz/
    ├── fuzz_superblock.cpp
    ├── fuzz_header.cpp
    ├── fuzz_keyfile.cpp
    └── fuzz_manifest.cpp
```

---

## Mandatory Test Matrix (Requirements)

### Crypto

| ID | Test | Type | File |
|----|------|------|------|
| C-01 | Known-answer AEAD encrypt/decrypt | Unit | `tests/unit/crypto/test_aead.cpp` |
| C-02 | Known-answer Argon2id KDF | Unit + Vector | `tests/unit/crypto/test_kdf.cpp` |
| C-03 | HKDF chunk key derivation vectors | Unit | `tests/unit/crypto/test_hkdf.cpp` |
| C-04 | Secure random non-zero failure injection | Unit | `tests/unit/crypto/test_random.cpp` |
| C-05 | Constant-time compare timing smoke | Unit | `tests/unit/crypto/test_consttime.cpp` |
| C-06 | Secure buffer wipe on destroy | Unit | `tests/unit/crypto/test_secure_buffer.cpp` |

### Header

| ID | Test | Type | File |
|----|------|------|------|
| H-01 | Header parse/write roundtrip | Unit | `tests/unit/container/test_header_codec.cpp` |
| H-02 | Wrong-password detection generic error | Integration | `tests/integration/wrong_password.cpp` |
| H-03 | Primary/backup header sync on create | Unit | `tests/unit/container/test_dual_header.cpp` |
| H-04 | Superblock CRC validation + total size == 512 | Unit | `tests/unit/container/test_superblock.cpp` |
| H-05 | Backup header restore | Integration | `tests/integration/header_backup_restore.cpp` |

### Keyfile

| ID | Test | Type | File |
|----|------|------|------|
| K-01 | Single keyfile unlock | Integration | `tests/integration/keyfile.cpp` |
| K-02 | Multiple keyfiles order sensitivity | Unit | `tests/unit/keyfile/test_combiner.cpp` |
| K-03 | keyfile_required enforcement | Integration | `tests/integration/keyfile.cpp` |
| K-04 | Keyfile max size truncation 512KiB | Unit | `tests/unit/keyfile/test_format.cpp` |

### Hidden Volume

Authoritative IDs (match `hidden-volume-design.md`):

| ID | Test | Type | File |
|----|------|------|------|
| HV-01 | Create hidden in empty outer | Integration | `tests/integration/hidden_volume.cpp` |
| HV-02 | Hidden unlock lists empty root | Integration | `tests/integration/hidden_volume.cpp` |
| HV-03 | Outer put near boundary fails with protect | Integration | `tests/integration/hidden_protection.cpp` |
| HV-04 | Outer put without protect blocked at hidden_offset (bitmap) | Integration | `tests/integration/hidden_boundary.cpp` |
| HV-05 | Hidden put/get roundtrip | Integration | `tests/integration/hidden_volume.cpp` |
| HV-06 | Wrong hidden password fails generic error | Integration | `tests/integration/wrong_password.cpp` |
| HV-07 | Outer + hidden different VMK verify | Unit | `tests/unit/vault/test_hidden_keys.cpp` |
| HV-08 | Create hidden when outer chunk overlaps → error | Integration | `tests/integration/hidden_boundary.cpp` |
| HV-09 | Outer backup header restore preserves hidden sector | Integration | `tests/integration/header_backup_restore.cpp` |
| HV-10 | Repair outer does not destroy hidden ciphertext | Integration | `tests/integration/repair.cpp` |

### Manifest

| ID | Test | Type | File |
|----|------|------|------|
| M-01 | Empty manifest create | Unit | `tests/unit/vault/test_manifest.cpp` |
| M-02 | Directory tree serialize/deserialize | Unit | `tests/unit/vault/test_manifest.cpp` |
| M-03 | Manifest generation monotonic | Unit | `tests/unit/vault/test_manifest.cpp` |
| M-04 | Manifest AEAD tamper detection | Unit | `tests/unit/vault/test_manifest.cpp` |
| M-05 | Reject inode #200 (`ManifestFull`) | Unit | `tests/unit/vault/test_manifest_cap.cpp` |
| M-06 | `manifest_root_hash` golden vector | Unit | `tests/unit/vault/test_manifest_hash.cpp` |

### File Operations

| ID | Test | Type | File |
|----|------|------|------|
| F-01 | put/get small file | Integration | `tests/integration/file_ops.cpp` |
| F-02 | put/get large file multi-chunk | Integration | `tests/integration/file_ops.cpp` |
| F-03 | list directory | Integration | `tests/integration/file_ops.cpp` |
| F-04 | remove file | Integration | `tests/integration/file_ops.cpp` |
| F-05 | rename file | Integration | `tests/integration/file_ops.cpp` |
| F-06 | mkdir nested | Integration | `tests/integration/file_ops.cpp` |
| F-07 | remove recursive dir | Integration | `tests/integration/file_ops.cpp` |
| F-08 | Unicode paths UTF-8 | Integration | `tests/integration/file_ops_unicode.cpp` |

### Recovery & Corruption

| ID | Test | Type | File |
|----|------|------|------|
| R-01 | Interrupted-write journal replay | Integration | `tests/integration/interrupted_write.cpp` |
| R-02 | Corrupted primary header → backup | Integration | `tests/integration/corrupt_header.cpp` |
| R-03 | Truncated vault detection | Integration | `tests/integration/truncated_vault.cpp` |
| R-04 | verify structural (locked) pass | Integration | `tests/integration/verify.cpp` |
| R-04b | verify crypto shallow (unlocked) pass | Integration | `tests/integration/verify.cpp` |
| R-05 | verify deep chunk failure | Integration | `tests/integration/verify.cpp` |
| R-06 | repair dry-run vs apply | Integration | `tests/integration/repair.cpp` |

### Fuzz

| ID | Test | Type | File |
|----|------|------|------|
| Z-01 | Fuzz superblock parser | Fuzz | `tests/fuzz/fuzz_superblock.cpp` |
| Z-02 | Fuzz encrypted header blob | Fuzz | `tests/fuzz/fuzz_header.cpp` |
| Z-03 | Fuzz keyfile combiner inputs | Fuzz | `tests/fuzz/fuzz_keyfile.cpp` |
| Z-04 | Fuzz manifest page parser | Fuzz | `tests/fuzz/fuzz_manifest.cpp` |

### Cross-Platform & Portable

| ID | Test | Type | File |
|----|------|------|------|
| P-01 | Path normalization Windows/POSIX | Unit | `tests/unit/platform/test_paths.cpp` |
| P-02 | Portable mode INI detection | Unit | `tests/unit/portable/test_detect.cpp` |
| P-03 | no_host_traces log disabled | Unit | `tests/unit/portable/test_no_traces.cpp` |
| P-04 | Temp cleanup on exit | Integration | `tests/integration/portable_cleanup.cpp` |

### Mount (M9 — `SV_BUILD_MOUNT=ON`)

| ID | Test | Type | File |
|----|------|------|------|
| MT-01 | Vault put → OS read roundtrip | Integration | `tests/integration/test_mount.cpp` |
| MT-02 | SvFs read-only denies write | Integration | `tests/integration/test_svfs.cpp` |
| MT-03 | Dismount releases session | Integration | `tests/integration/test_mount.cpp` |
| MT-04 | Mount stress 100 small files | Integration | `tests/integration/test_mount.cpp` |
| MT-05 | Auto-dismount idle | Integration | `tests/integration/test_mount.cpp` |
| MT-06 | Hidden protection via SvFs | Integration | `tests/integration/test_svfs.cpp` |
| MT-07 | OS write → vault get roundtrip | Integration | `tests/integration/test_mount.cpp` |

---

## Test Data

| Asset | Description |
|-------|-------------|
| `vectors/kdf_argon2id_v1.json` | RFC-style KDF vectors |
| `vectors/aead_xchacha_v1.json` | libsodium-compatible AEAD |
| `vectors/header_golden_v1.bin` | Golden encrypted header |
| `fixtures/empty_64MiB.svlt` | Pre-generated test vault (test-only password) |

**Test password convention:** `test-vector-password-do-not-use` (documented in fixtures README).

---

## CI Requirements

| Gate | Threshold |
|------|-----------|
| Unit tests | 100% pass |
| Integration tests | 100% pass |
| Coverage (goal) | ≥80% line on `sv_crypto`, `sv_container`, `sv_vault` |
| Fuzz | 60s smoke per target nightly |
| ASAN build | Linux Debug weekly |

---

## Performance Benchmarks (Non-gating)

| Bench | Target |
|-------|--------|
| Argon2id desktop profile | < 3s on ref i7 |
| AEAD 1 MiB chunk | > 500 MB/s AES-NI system |

`sv benchmark` command mirrors `tests/unit/crypto/bench_kdf.cpp`.

---

## Test Execution

```bash
cmake -B build -DSV_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Single suite
ctest --test-dir build -R hidden_volume

# Fuzz (local)
./build/tests/fuzz/fuzz_header -runs=10000 corpus/
```

---

## Regression Policy

Every bug fix requires a test that fails before fix. PR template checkbox: "Test added for regression."

---

## Manual Test Checklist (Pre-Release)

- [ ] Create 10G vault on USB, lock, re-unlock
- [ ] Change password, old fails new succeeds
- [ ] Hidden volume coercion scenario walkthrough
- [ ] Portable no_host_traces: verify no registry keys
- [ ] Windows + Linux cross-open same `.svlt`

---

*Self-contained test plan for Shadow Vault.*