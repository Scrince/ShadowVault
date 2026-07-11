# Phase 1 — PBA Prototype Implementation Plan

| Field | Value |
|-------|-------|
| **Date** | 2026-07-06 |
| **Status** | Phase 1 lab complete (host + QEMU + USB staging); physical laptop execution pending |
| **Duration** | 16 weeks (RFC estimate) |
| **Parent** | `docs/rfc-system-encryption.md` |

---

## Goal

Deliver a lab-only **UEFI Pre-Boot Authentication** prototype that unlocks a test partition and chain-loads the OS bootloader. No in-place system encryption in Phase 1.

## Exit Criteria

| ID | Criterion |
|----|-----------|
| P1-01 | `ShadowVaultBoot.efi` loads on UEFI firmware (QEMU OVMF + physical lab machine) |
| P1-02 | Passphrase unlock derives SVMK from triple-sector system header |
| P1-03 | Wrong passphrase returns uniform error (R-009) |
| P1-04 | Successful unlock chain-loads `bootmgfw.efi` or GRUB shim on test disk |
| P1-05 | Read-only access to a pre-encrypted test `.svlt` or test FAT partition from PBA menu (stretch) |
| P1-06 | Production installer: MSI/ZIP via `scripts/build-installer.ps1`; signed EFI optional for lab |

---

## Work Breakdown

### Sprint A (weeks 1–4): `sv_pba_core`

| Task | Output |
|------|--------|
| Extract freestanding crypto subset | `src/sv_pba/core/` — Argon2id + XChaCha + HKDF (no `malloc`, no OS CRT) |
| System header parser | Parse 4 KiB triple-sector layout (reuse `header_codec` logic, freestanding) |
| VMK unwrap API | `sv_pba_unlock(system_header, credentials) -> svmk` |
| Host tests | Run `sv_pba_core` tests on desktop against golden vectors |

### Sprint B (weeks 5–8): EFI shell

| Task | Output |
|------|--------|
| EDK2 / gnu-efi toolchain | Documented build in `src/sv_pba/efi/README.md` |
| Framebuffer/text UI | Minimal passphrase prompt |
| Link `sv_pba_core` | Static link into `ShadowVaultBoot.efi` |
| QEMU harness | `scripts/pba-qemu.sh` boots OVMF + test ESP |

### Sprint C (weeks 9–12): Chain-load

| Task | Output |
|------|--------|
| ESP layout tool | Place `ShadowVaultBoot.efi` before Windows Boot Manager |
| VMK handoff spike | Platform-specific sealed memory or filter driver stub (OQ-S01) |
| Lab machine test | Physical UEFI boot on test laptop |

### Sprint D (weeks 13–16): Hardening

| Task | Output |
|------|--------|
| Constant-time audit | SE-01 equivalent for PBA |
| Memory wipe | Zero KDF buffers before chain-load |
| Rescue path | Boot menu "Recovery" → detect + chain-load `ShadowVaultRescue.efi` |
| VMK handoff spike | BS-only UEFI variable `SvPbaVmkHandoff` (D-15) |
| Documentation | OQ-S01 recorded in `docs/DECISIONS.md` |

---

## Repository Layout (new)

```
src/sv_pba/
  CMakeLists.txt          # SV_BUILD_PBA=OFF by default
  README.md
  core/                   # Freestanding unlock library (host-testable)
  efi/                    # ShadowVaultBoot.efi (EDK2)
  host_tests/             # Catch2 tests for core (not EFI)
```

---

## Dependencies

| Dependency | Notes |
|------------|-------|
| v1.2.0 released | M9 mount stable |
| EDK2 or gnu-efi | Phase 1 toolchain (not vendored in v1.2 tree) |
| QEMU + OVMF | CI / dev harness |
| Code signing cert | Optional Phase 1; required before beta |

---

## Out of Scope (Phase 1)

- In-place encryption service
- TPM sealing
- Hidden OS partition
- ~~Windows installer integration~~ (v1.2 MSI/ZIP — D-16)
- Rescue USB builder (Phase 3)

---

## References

- `docs/rfc-system-encryption.md`
- `docs/threat-model-system.md`
- `docs/m10-architecture-review.md`
- `docs/key-hierarchy.md`