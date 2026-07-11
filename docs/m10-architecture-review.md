# Milestone 10 — Architecture Review Record

| Field | Value |
|-------|-------|
| **Date** | 2026-07-06 |
| **Status** | Approved (design gate) |
| **Milestone** | M10 — System Encryption Plan |
| **Implementation gate** | No kernel/driver/firmware code until Phase 1 milestone opens |

---

## Purpose

Formal architecture review for Shadow Vault **system encryption** (full-disk / OS volume). This milestone is **design only**; it validates that future implementation can proceed without rework of container format, key hierarchy, or rescue tooling.

---

## Documents Reviewed

| Document | Path | Verdict |
|----------|------|---------|
| System encryption RFC | `docs/rfc-system-encryption.md` | Approved |
| System threat model addendum | `docs/threat-model-system.md` | Approved |
| Rescue media specification | `docs/rescue-media-spec.md` | Approved |
| Key hierarchy (system extension) | `docs/key-hierarchy.md` + RFC §Key Hierarchy | Approved |
| Risk register (R-026–R-030) | `docs/risk-register.md` | Approved |

---

## Review Findings

### Approved architecture

1. **UEFI-first PBA** — Minimal EFI loader decrypts triple-sector system headers and chain-loads OS bootloader; legacy BIOS documented as unsupported in initial phase.
2. **Crypto reuse** — System VMK (SVMK) uses existing Argon2id + XChaCha20-Poly1305 + HKDF chunk/sector derivation; no new primitives required.
3. **Interrupted encryption** — Journal replay pattern mirrors container manifest journal (`PLAINTEXT` → `ENCRYPTING` → `ENCRYPTED`); PBA blocks boot during unsafe states unless rescue mode.
4. **Rescue USB** — GPT layout with dedicated ESP + data partition; offline `svcli` header restore parity with `.svlt` workflows.
5. **Container coexistence** — `.svlt` volumes remain independent VMKs; system unlock does not subsume file-container credentials.

### Deferred / open questions (non-blocking for gate)

| ID | Topic | Resolution target |
|----|-------|-------------------|
| OQ-S01 | Windows driver stack vs EFI-only | Phase 1 spike (+3 months) |
| OQ-S02 | Linux LUKS coexistence | Phase 1 spike (+2 months) |
| OQ-S03 | Hidden OS boot menu exposure | Phase 4 threat review |

### Explicit non-goals confirmed

- No VeraCrypt/TrueCrypt partition compatibility
- No custom kernel block filter in Phase 1
- No macOS system encryption in this program
- No TPM-only unlock without passphrase in Phase 1

---

## Effort Estimate

Per `docs/rfc-system-encryption.md` §Phased Implementation Plan:

| Phase | Duration | Cumulative |
|-------|----------|------------|
| 0 — Design (M10) | 4 weeks | 4 weeks |
| 1 — PBA prototype | 16 weeks | 20 weeks |
| 2 — In-place encrypt | 20 weeks | 40 weeks |
| 3 — Rescue & recovery | 12 weeks | 52 weeks |
| 4 — Hidden system | 16+ weeks | 68+ weeks |

**Total: 12–18 months** after M9 (v1.2) completion.

---

## M10 Acceptance Criteria

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Architecture review completed | **Pass** | This document |
| Risk register updated | **Pass** | R-026 through R-030 in `docs/risk-register.md` |
| 12+ month effort estimate | **Pass** | RFC phased plan (52–68 weeks engineering) |
| No kernel/driver code merged | **Pass** | Repository contains design docs only for system encryption |

---

## Implementation Gate

Phase 1 (PBA prototype) may begin only when:

1. v1.2 (M9 mount) is released and stable on Windows + Linux FUSE paths.
2. Rescue USB builder spec is ticketed with signing pipeline owner.
3. Secure Boot signing certificate path is resolved (OQ-06 extension).

---

## References

- `docs/rfc-system-encryption.md`
- `docs/threat-model-system.md`
- `docs/rescue-media-spec.md`
- `docs/risk-register.md`
- `docs/milestone-plan.md` — M10 section

---

*M10 design gate — approved 2026-07-06. No system encryption implementation in tree.*