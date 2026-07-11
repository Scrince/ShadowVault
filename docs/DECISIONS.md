# Shadow Vault — Recorded Design Decisions

| Field | Value |
|-------|-------|
| **Date** | 2026-07-05 |
| **Status** | Active |
| **Authority** | Resolves open questions blocking M1 |

---

## Pre-M1 Decisions

| ID | Decision | Rationale | Date |
|----|----------|-----------|------|
| D-01 | **GUI toolkit: Qt 6 LTS** | Fastest cross-platform path; LGPL with dynamic linking | 2026-07-05 |
| D-02 | **License: GPLv3** | Copyleft appropriate for security-sensitive crypto product; aligns with WinFsp interaction review | 2026-07-05 |
| D-03 | **Password policy:** minimum 12 characters; empty password **never** allowed even with keyfiles | Keyfiles supplement password, not replace it (resolves OQ-04) | 2026-07-05 |
| D-04 | **Manifest v1: flat sorted inode list** | Simplest correct implementation; **199 inode cap**; B+ tree deferred to v1.1 (resolves OQ-02) | 2026-07-05 |
| D-05 | **Three header sectors** | Primary OUTER + backup OUTER + hidden HIDDEN at `0x2200` (resolves backup slot conflict) | 2026-07-05 |
| D-06 | **Header nonce: 24-byte cleartext prefix** | Standard AEAD bootstrap; libsodium XChaCha compatible | 2026-07-05 |
| D-07 | **Credential mixer: HMAC-SHA256 chain** | Normative; see `key-hierarchy.md` | 2026-07-05 |
| D-08 | **KDF v1: Argon2id only** | scrypt/PBKDF2 deferred to v1.1 | 2026-07-05 |
| D-09 | **Sub-key derivation: HKDF-SHA256 in `hkdf.cpp`** | Explicit info strings; not `crypto_kdf_derive_from_key` | 2026-07-05 |
| D-10 | **Deniability default: standard profile** | `deniability_profile=0`; strict opt-in | 2026-07-05 |
| D-11 | **Tail superblock non-overlap** | `container_size = data_offset + data_size + 512`; tail not part of data area | 2026-07-05 |
| D-12 | **Mount backend v1: WinFsp FUSE3 (Windows), libfuse3 (Linux)** | Single `fuse_backend.cpp` adapter over SvFs; no custom kernel driver | 2026-07-06 |
| D-13 | **Inode `data_generation` field (340-byte inode)** | Chunk AEAD binds manifest generation at write time; required for multi-file reads after manifest bump; max inodes 192 | 2026-07-06 |
| D-14 | **Release v1.2.0** | M9 mount backends + M10 design gate; container format revision (inode 340 B) is backward-compatible read for `data_generation=0` | 2026-07-06 |
| D-15 | **VMK handoff (OQ-S01): BS-only UEFI variable** | Phase 1 spike: `SvPbaVmkHandoff` GUID + 64-byte blob; cleared on `ExitBootServices`; Phase 2 filter driver reads before handoff | 2026-07-06 |
| D-16 | **Windows installer: WiX MSI + portable ZIP** | Per-machine MSI via `scripts/build-installer.ps1`; PATH updated for `svcli`; optional PBA feature; unsigned v1 (OQ-06) | 2026-07-06 |

---

## Interim Defaults (Non-Blocking)

| ID | Topic | Interim Default |
|----|-------|-----------------|
| OQ-03 | Volume size preset | None in CLI; wizard suggests 10G/20G in M8 |
| OQ-06 | Code signing | Unsigned v1 |
| OQ-07 | Max keyfiles | 64 |
| OQ-09 | FIPS mode | Not in v1 |
| OQ-10 | Chunk size | Fixed 1 MiB |

---

## References

- `file-format-spec.md` — normative on-disk layout
- `SHADOW-VAULT-DESIGN.md` — Key Decisions KD-01–KD-20

---

*Update this file when resolving additional open questions.*