# Shadow Vault — Key Hierarchy

| Field | Value |
|-------|-------|
| **Date** | 2026-07-05 |
| **Status** | Draft |
| **Revision** | 2026-07-05 — review 8e7d24de |

---

## Overview

Shadow Vault uses a layered key hierarchy: user credentials derive a **header key** via KDF; the header key decrypts a randomly generated **Volume Master Key (VMK)** in the encrypted header. The VMK derives sub-keys for manifest, journal, and chunks via **HKDF-SHA256** implemented in `src/sv_crypto/hkdf.cpp`.

---

## Credential Mixer (Normative)

Produces 32-byte `kdf_secret` fed to Argon2id:

```text
step0 = SHA256(password_utf8_nfc)
for each keyfile_hash[i] in lexicographic path order:
    step{i+1} = HMAC-SHA256(step{i}, keyfile_hash[i])
kdf_secret = final step
```

If no keyfiles: `kdf_secret = SHA256(password_utf8_nfc)`.

**Password policy:** Minimum 12 characters; empty password never allowed (D-03).

**Implementation:** `src/sv_keyfile/combiner.cpp`, `src/sv_vault/credential_material.cpp`.

---

## KDF Layer — Argon2id (v1 only)

| Parameter | Default | Portable |
|-----------|---------|----------|
| `m_cost` | 262144 KiB | 65536 KiB |
| `t_cost` | 3 | 2 |
| `p_cost` | 1 | 1 |
| `salt` | `outer_salt` or `hidden_salt` (32 B, superblock) | same |
| `output_len` | 64 bytes | 64 bytes |

**Output usage:**

| Bytes | Purpose |
|-------|---------|
| 0–31 | `header_key` — AEAD decrypt of header sector |
| 32–63 | **Unused in v1** (reserved; zero on write) |

Confirmation uses `HMAC-SHA256(header_key, "sv-confirm-v1")` stored as `header_key_confirm` in header plaintext — **not** derived from bytes 32–63.

---

## Header Decryption Bootstrap

1. Derive `header_key` from `kdf_secret` + salt.
2. Read **cleartext 24-byte `header_nonce`** from sector offset 0 (see `file-format-spec.md`).
3. AEAD-decrypt using:
   - **Algorithm:** `crypto_aead_xchacha20poly1305_ietf`
   - **Key:** `header_key` (32 bytes)
   - **Nonce:** cleartext `header_nonce` (24 bytes)
   - **AAD:** 80-byte layout (`magic || format_version || slot_id || slot_type || reserved || outer_salt || hidden_salt`)
4. Verify `header_key_confirm` constant-time.
5. Extract `volume_master_key`.

---

## VMK Sub-Key Derivation — HKDF-SHA256

Implemented explicitly in `hkdf.cpp` using libsodium `crypto_kdf_hkdf_sha256_expand` / equivalent — **not** `crypto_kdf_derive_from_key`.

| Sub-Key | HKDF `info` string | Output |
|---------|-------------------|--------|
| Manifest page | `"sv-mp-v1" \|\| generation_u64_le \|\| page_index_u64_le` | 32 B |
| Journal record | `"sv-jr-v1" \|\| sequence_u64_le` | 32 B |
| File chunk | `"sv-ch-v1" \|\| chunk_id_u64_le` | 32 B |

```cpp
// src/sv_crypto/hkdf.cpp — normative pattern
AeadKey hkdf_chunk_key(Span<const uint8_t> vmk, uint64_t chunk_id) {
    uint8_t info[16];
    memcpy(info, "sv-ch-v1", 8);
    store_le64(info + 8, chunk_id);
    return hkdf_sha256(vmk, /*salt=*/{}, info);
}
```

Test vectors: `tests/vectors/hkdf_chunk_v1.json`.

---

## Chunk Nonce Derivation

Inode stores random **24-byte `base_nonce`**. Per-chunk nonce (not stored on disk):

```text
chunk_nonce = BLAKE2b-256(base_nonce || chunk_index_u64_le)[0:24]
```

Uses libsodium `crypto_generichash_blake2b` with 24-byte output.

---

## Hidden vs Outer Keys

| Volume | Salt | Header sector | VMK |
|--------|------|---------------|-----|
| Outer | `outer_salt` | Primary `0x200` + backup `0x1200` | VMK_outer |
| Hidden | `hidden_salt` | Hidden `0x2200` | VMK_hidden |

Independent passwords required. Hidden VMK unrelated to outer VMK.

---

## Change Password Flow

1. Unlock → obtain VMK.
2. Derive new `header_key'` from new credentials.
3. Re-encrypt header payload (same VMK).
4. Write backup sector, then primary (outer); hidden sector for hidden password change.
5. Update salt in superblock (+ tail copy).

---

## Header Backup File (`.svhdr`)

| Field | Size |
|-------|------|
| magic `SVHB` | 4 |
| format_version | 2 |
| `slot_id` | 2 (`1`/`2` outer, `3` hidden) |
| `slot_type` | 1 |
| reserved | 1 |
| `outer_salt` | 32 |
| `hidden_salt` | 32 |
| encrypted header sector (4096 B) | 4096 |
| crc32 | 4 |

---

## KeyManager Interface

```cpp
namespace sv::vault {

class KeyManager {
public:
    Result<DerivedKeys> derive_from_credentials(
        const CredentialMaterial& cred,
        const container::Superblock& sb,
        HeaderSlotIntent intent);

    Result<EncryptedHeader> seal_header(
        const HeaderPayload& payload,
        const DerivedKeys& keys,
        const container::Superblock& sb,
        HeaderSlot slot);

    Result<HeaderPayload> open_header(
        const EncryptedHeaderSector& enc,
        const DerivedKeys& keys,
        const container::Superblock& sb);
};

struct DerivedKeys {
    SecureBuffer header_key;  // 32 bytes; only used bytes from KDF
};

} // namespace sv::vault
```

**Module:** `src/sv_vault/key_manager.cpp` — owned by `VaultEngine`.

---

*Self-contained key hierarchy reference for Shadow Vault.*