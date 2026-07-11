# Shadow Vault — `.svlt` File Format Specification

| Field | Value |
|-------|-------|
| **Format Name** | Shadow Vault Container |
| **Extension** | `.svlt` |
| **Magic** | `SVLT` |
| **Format Version** | 1 |
| **Date** | 2026-07-05 |
| **Status** | v1.0 frozen (normative, 2026-07-05) |
| **Revision** | 2026-07-05 — review 8e7d24de (pass 3: tail geometry) |

> **Normative authority:** This document is the single source of truth for on-disk layout. Other docs reference this file; they must not contradict it.

---

## Overview

The `.svlt` format is a single-file encrypted container with:

- Cleartext superblock (primary + tail backup)
- **Three** encrypted header sectors (outer primary, outer backup, hidden)
- Transaction journal, encrypted manifest, outer data area
- Optional hidden volume region at the end of the outer data area

**Endianness:** All multi-byte integers are **little-endian**.  
**Alignment:** 512-byte sector alignment for region boundaries.  
**AEAD:** `crypto_aead_xchacha20poly1305_ietf` (libsodium); **24-byte nonce** (`crypto_aead_xchacha20poly1305_ietf_NPUBBYTES`).

---

## Region Map (v1 defaults)

The **hidden volume is a suffix inside `data_size`**. The **tail superblock is outside `data_size`** — it adds 512 bytes to the total file.

```
container_size = data_offset + data_size + 512

Offset               Size                  Content
────────────────────────────────────────────────────────────────────────────
0x00000000           512 B                 Cleartext Superblock (primary)
0x00000200           4096 B                Primary Encrypted Header (OUTER)
0x00001200           4096 B                Backup Encrypted Header (OUTER)
0x00002200           4096 B                Hidden Encrypted Header (HIDDEN)
0x00003200           journal_size          Transaction / Recovery Journal
journal_end          manifest_size         Encrypted Volume Manifest
manifest_end         data_size             Data area [data_offset .. data_offset+data_size)
  data_offset .. hidden_offset            Decoy / outer file chunks
  hidden_offset .. data_offset+data_size    Hidden region suffix (when hidden_max_size > 0)
data_offset+data_size  512 B                 Cleartext Superblock (tail backup)
────────────────────────────────────────────────────────────────────────────
```

**Allocator rule:** Chunk I/O, hidden sub-layout, and bitmap indices use **`[data_offset, data_offset + data_size)` only**. Bytes `[data_offset + data_size, container_size)` are **tail superblock only** — no chunks, no hidden data.

**Key formulas:**

| Symbol | Formula |
|--------|---------|
| `container_size` | `data_offset + data_size + 512` (total on-disk file size) |
| `data_size` | `container_size − data_offset − 512` (at create from user `--size`) |
| `decoy_size` | `data_size − hidden_max_size` (decoy-only bytes) |
| `hidden_offset` | `data_offset + decoy_size` = `data_offset + data_size − hidden_max_size` |
| `hidden_region_size` | `hidden_max_size` (0 if no hidden volume) |
| `tail_superblock_offset` | `container_size − 512` = `data_offset + data_size` |

When `hidden_max_size = 0`, the entire `data_size` is decoy space and `hidden_offset = data_offset + data_size` (abuts tail superblock).

Bytes **512–1023** are **unused** in v1 (gap between primary superblock and primary header).

### Worked Example — 10 GiB container, 2 GiB hidden

User `sv create --size 10G` → **`container_size = 10 GiB`** (10,737,418,240 bytes).  
Assume `journal_size = 64 MiB`, `manifest_size = 128 MiB`, `hidden_max_size = 2 GiB`:

| Region | Offset (hex) | Size |
|--------|----------------|------|
| Primary superblock | `0x0` | 512 B |
| Headers (×3) | `0x200` | 12 KiB |
| Journal | `0x3200` | 64 MiB |
| Manifest | `0x402000` | 128 MiB |
| `data_offset` | `0xC02000` (12,582,912) | — |
| `data_size` | — | `container_size − 0xC02000 − 512` = **10,724,834,816 B** (≈ 9.988 GiB) |
| `decoy_size` | — | `data_size − 2 GiB` = **8,577,351,168 B** (≈ 7.988 GiB) |
| `hidden_offset` | `0x1FFFFFE00` (8,589,934,080) | `data_offset + decoy_size` |
| Hidden region | `[hidden_offset, data_offset+data_size)` | **2 GiB** (2,147,483,648 B) |
| Data area end | `data_offset + data_size` = `0x27FFFFE00` | = `container_size − 512` |
| Tail superblock | `container_size − 512` = `0x27FFFFE00` | 512 B |
| **`container_size`** | — | **10 GiB** (matches `--size`) |

**Check:** `0xC02000 + 10,724,834,816 + 512 = 10,737,418,240` ✓

---

## Cleartext Superblock (512 bytes)

**Copies:** offset `0` (primary) and offset `container_size − 512` (tail backup). Both copies must be identical on every committed write.

**Size invariant:** Field table sums to **512 bytes exactly** (`0xD0` + 304 reserved = `0x200` = 512).

**CRC:** CRC32-IEEE covers bytes **`0x00–0xCB`** (all fields through `reserved_c`, excluding `crc32` and trailing `reserved`).

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | `magic` | ASCII `SVLT` |
| 0x04 | 2 | `format_version` | `1` |
| 0x06 | 2 | `superblock_size` | `512` |
| 0x08 | 4 | `flags` | Bit flags (see below) |
| 0x0C | 8 | `container_size` | Total on-disk file size (`data_offset + data_size + 512`) |
| 0x14 | 8 | `primary_header_offset` | Default `0x200` |
| 0x1C | 4 | `primary_header_size` | `4096` |
| 0x20 | 8 | `backup_header_offset` | Default `0x1200` |
| 0x28 | 4 | `backup_header_size` | `4096` |
| 0x2C | 8 | `hidden_header_offset` | Default `0x2200` |
| 0x34 | 4 | `hidden_header_size` | `4096` |
| 0x38 | 8 | `journal_offset` | Default `0x3200` |
| 0x40 | 8 | `journal_size` | Length in bytes |
| 0x48 | 8 | `manifest_offset` | Absolute offset |
| 0x50 | 8 | `manifest_size` | Manifest region size |
| 0x58 | 8 | `data_offset` | Outer data area start |
| 0x60 | 8 | `data_size` | Outer data area size |
| 0x68 | 32 | `outer_salt` | KDF salt for outer headers |
| 0x88 | 32 | `hidden_salt` | KDF salt for hidden header |
| 0xA8 | 16 | `outer_kdf_id` | UUID for outer KDF params |
| 0xB8 | 16 | `hidden_kdf_id` | UUID for hidden KDF params |
| 0xC8 | 1 | `deniability_profile` | `0`=standard, `1`=strict (see § Deniability) |
| 0xC9 | 3 | `reserved_c` | Zero |
| 0xCC | 4 | `crc32` | CRC32-IEEE of bytes `0x00–0xCB` |
| 0xD0 | 304 | `reserved` | Zero (through byte 511) |

### Superblock `flags` (uint32)

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `CREATED` | Container initialized |
| 1 | `QUICK_FORMAT` | Data area may be sparse |
| 2 | `HIDDEN_CONFIGURED` | Hidden volume created (see deniability profile) |
| 3 | `JOURNAL_DIRTY` | Replay required on unlock |
| 4–31 | — | Reserved (zero on write) |

**`HIDDEN_CONFIGURED` (bit 2):** Written only when `deniability_profile=0` (standard). Strict profile (`1`) never sets this bit.

### Superblock Layout Validation

On read and after `create`, validators **must** check:

1. `magic == SVLT` and `format_version` supported
2. `superblock_size == 512`
3. `crc32` matches bytes `0x00–0xCB` (primary and tail copies must match)
4. `container_size >= data_offset + data_size + 512`
5. **`container_size == data_offset + data_size + 512`** (exact, v1)
6. `tail_superblock_offset == container_size − 512 == data_offset + data_size`
7. No region overlap among headers, journal, manifest, `[data_offset, data_offset+data_size)`, and tail
8. `hidden_offset + hidden_max_size <= data_offset + data_size` when hidden configured

---

## Encrypted Header Sectors (4096 bytes each)

Three independent sectors; each uses the same on-disk envelope.

| Sector | Offset | `slot_id` | `slot_type` | Purpose |
|--------|--------|-----------|-------------|---------|
| Primary | `0x200` | `1` | `1` OUTER | Active outer header |
| Backup | `0x1200` | `2` | `1` OUTER | Outer redundancy / recovery |
| Hidden | `0x2200` | `3` | `2` HIDDEN | Hidden volume header |

### Hidden Header Sector — Initial State at Outer `create`

Before `create-hidden`, the hidden sector is **preallocated but uninitialized**:

| Bytes | Value |
|-------|-------|
| `0–23` | Random `header_nonce` |
| `24–3575` | Zero ciphertext |
| `3576–3591` | Zero tag |
| `3592–4079` | Zero reserved |
| `4080–4083` | `header_slot_crc32` over bytes `0–4079` |

Hidden unlock on uninitialized sector: AEAD auth fails → generic `WrongCredentials` after full KDF (T13). Forensic note: initialized hidden header has valid Poly1305 tag; pre-create-hidden sector has zero tag.

**Encryption:** XChaCha20-Poly1305 IETF (`aead_id=1`).  
**Key:** `header_key` from Argon2id (see `key-hierarchy.md`).  
**Nonce:** 24-byte **cleartext prefix** before ciphertext (not inside plaintext).

### AEAD Associated Data (80 bytes, exact layout)

| AAD offset | Size | Content |
|------------|------|---------|
| 0 | 4 | `magic` (`SVLT`) |
| 4 | 2 | `format_version` (uint16) |
| 6 | 2 | `slot_id` (uint16): `1`=primary, `2`=backup, `3`=hidden |
| 8 | 1 | `slot_type` (uint8): `1`=OUTER, `2`=HIDDEN |
| 9 | 7 | `reserved` (zero) |
| 16 | 32 | `outer_salt` |
| 48 | 32 | `hidden_salt` |

**Total: 80 bytes.** Use `crypto_aead_xchacha20poly1305_ietf` with this exact AAD.

### Encrypted Header On-Disk Envelope (4096 bytes)

| Slot offset | Size | Field |
|-------------|------|-------|
| 0 | 24 | `header_nonce` — cleartext random, used for AEAD |
| 24 | 3552 | `ciphertext` — AEAD plaintext (3552 bytes) |
| 3576 | 16 | `tag` — Poly1305 |
| 3592 | 488 | `reserved` — zero |
| 4080 | 4 | `header_slot_crc32` — CRC32-IEEE of bytes `0–4079` |
| 4084 | 12 | `crc_reserved` — zero |

**Bootstrap rule:** Decrypt using cleartext `header_nonce`; never read nonce from inside ciphertext.

### AEAD Plaintext Payload (3552 bytes)

Active v1 fields in first 512 bytes:

| Pt offset | Size | Field | Notes |
|-----------|------|-------|-------|
| 0x00 | 2 | `payload_version` | `1` |
| 0x02 | 1 | `slot_type` | `1` OUTER, `2` HIDDEN |
| 0x03 | 1 | `kdf_type` | `1` = Argon2id only in v1 |
| 0x04 | 32 | `volume_master_key` | Random VMK |
| 0x24 | 32 | `header_key_confirm` | `HMAC-SHA256(header_key, "sv-confirm-v1")` |
| 0x44 | 8 | `manifest_offset` | Must match superblock |
| 0x4C | 8 | `manifest_size` | |
| 0x54 | 8 | `journal_offset` | Slot-relative for hidden header |
| 0x5C | 8 | `journal_size` | |
| 0x64 | 8 | `data_offset` | Slot-relative data start |
| 0x6C | 8 | `data_size` | Slot data area size |
| 0x74 | 4 | `chunk_size` | Default `1048576` |
| 0x78 | 1 | `aead_id` | `1` |
| 0x79 | 1 | `keyfile_required` | `0`/`1` |
| 0x7A | 2 | `reserved_a` | Zero |
| 0x7C | 64 | `kdf_params_blob` | Argon2id params |
| 0xBC | 8 | `manifest_generation` | Monotonic |
| 0xC4 | 8 | `journal_sequence` | Last committed seq |
| 0xCC | 32 | `manifest_root_hash` | BLAKE2b-256 of decrypted manifest |
| 0xEC | 8 | `hidden_offset` | **Outer only:** absolute hidden region start |
| 0xF4 | 8 | `hidden_max_size` | **Outer only** |
| 0xFC | 1 | `hidden_present` | **Outer only:** `0`/`1` |
| 0xFD | 3 | `reserved_b` | Zero |
| 0x100 | 8 | `protect_margin_bytes` | **Outer only:** uint64 |
| 0x108 | 8 | `hidden_journal_offset` | **Hidden only:** relative to `hidden_offset` |
| 0x110 | 8 | `hidden_journal_size` | **Hidden only:** default 4 MiB |
| 0x118 | 8 | `hidden_manifest_offset` | **Hidden only:** relative to `hidden_offset` |
| 0x120 | 8 | `hidden_manifest_size` | **Hidden only:** default 8 MiB |
| 0x128 | 8 | `hidden_data_offset` | **Hidden only:** relative to `hidden_offset` |
| 0x130 | 8 | `hidden_data_size` | **Hidden only** |
| 0x138 | 968 | `reserved` | Zero through byte 3551 |

**`kdf_type` v1:** Only value `1` (Argon2id) is valid. Values `2`/`3` reserved for v1.1.

### KDF Params Blob (64 bytes, Argon2id)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `m_cost` (KiB) |
| 4 | 4 | `t_cost` |
| 8 | 4 | `p_cost` |
| 12 | 4 | `parallelism_reserved` |
| 16 | 48 | reserved |

### Slot Write Policy

| Operation | Sectors updated |
|-----------|-----------------|
| `create` (outer) | Primary + backup OUTER |
| `change-password` (outer) | Primary + backup OUTER |
| `create-hidden` | Hidden sector + outer primary + outer backup |
| `change-password` (hidden) | Hidden sector only |

Outer backup **always** remains an OUTER header copy. Hidden header **never** overwrites backup.

---

## Wrong-Password & Corruption Detection

1. Derive `header_key` from credentials + appropriate salt.
2. Read cleartext `header_nonce` from target sector.
3. AEAD-decrypt ciphertext with 80-byte AAD.
4. Verify `header_key_confirm` constant-time.
5. Verify `header_slot_crc32`.
6. Cross-check offsets against superblock.
7. Return generic `WrongCredentials` on any cryptographic failure (no slot-type oracle).

**Unlock targets:**

| Intent | Sectors tried (in order) |
|--------|--------------------------|
| Outer | Primary OUTER → backup OUTER |
| Hidden | Hidden HIDDEN only |

---

## Deniability Profiles

| Profile | `deniability_profile` | `HIDDEN_CONFIGURED` | `hidden_present` (outer hdr) | Outer bitmap reservation |
|---------|----------------------|---------------------|------------------------------|--------------------------|
| **Standard** (v1 CLI default) | `0` | Set on `create-hidden` | `1` | Required |
| **Strict** (opt-in wizard) | `1` | Never set | `0` | Required |

**Both profiles** at `create-hidden`:

1. Create hidden header at `0x2200`.
2. Add `entry_type=4` (`HIDDEN_RESERVATION`) to outer manifest covering chunk range `[hidden_start_chunk, hidden_end_chunk)`.
3. Set outer `hidden_offset` / `hidden_max_size` in encrypted outer header (visible only with outer password).

Strict mode omits cleartext hints; boundary enforcement uses encrypted reservation + `hidden_offset` after outer unlock.

### Runtime `hidden_configured` (not an on-disk field)

After **outer unlock**, `VaultEngine` sets `hidden_configured = true` iff **any** of:

1. `hidden_max_size > 0` in decrypted outer header payload, **or**
2. Outer manifest inode page contains `entry_type = 4` (`HIDDEN_RESERVATION`).

Both are written at `create-hidden` in all profiles (including strict). In strict mode, (1) holds even when `hidden_present = 0` and `HIDDEN_CONFIGURED` superblock bit is unset.

`check_write_allowed()` uses `hidden_configured` for hard-boundary and margin checks. Hidden-only unlock does not set `hidden_configured` on the outer context.

---

## Transaction / Recovery Journal

### Journal Superblock (first 512 bytes of journal region)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `journal_magic` | `SVJR` |
| 4 | 4 | `journal_version` | `1` |
| 8 | 8 | `write_pointer` | Next append offset relative to `journal_offset + 512` |
| 16 | 8 | `last_committed_seq` | |
| 24 | 8 | `first_record_offset` | Usually `0` |
| 32 | 4 | `crc32` | Of bytes `0–31` |
| 36 | 476 | `reserved` | Zero |

### Journal Record On-Disk Layout

Records append sequentially from `journal_offset + 512`. Max record size: **65536** bytes. Records padded to **8-byte** alignment with zero bytes.

| Record offset | Size | Field |
|---------------|------|-------|
| 0 | 4 | `record_len` | Total bytes including this field |
| 4 | `record_len − 4` | `encrypted_body` | AEAD ciphertext + 16-byte tag |

**`encrypted_body` plaintext** (AEAD key from `hkdf_journal_key(VMK, sequence)`):

| Pt offset | Size | Field |
|-----------|------|-------|
| 0 | 8 | `sequence` |
| 8 | 2 | `op_type` |
| 10 | 2 | `reserved` |
| 12 | 4 | `payload_len` |
| 16 | `payload_len` | `payload` |

| `op_type` | Name | `payload` schema |
|-----------|------|------------------|
| 1 | `BEGIN` | `transaction_id` uuid (16 bytes) |
| 2 | `MANIFEST_WRITE` | `new_generation u64`, `page_index u32`, `page_byte_offset u64` |
| 3 | `CHUNK_ALLOC` | `chunk_id u64`, `inode_id u64` |
| 4 | `CHUNK_FREE` | `chunk_id u64` |
| 5 | `COMMIT` | `manifest_generation u64`, `manifest_root_hash[32]` |
| 6 | `ABORT` | empty (`payload_len=0`) |

**Example — COMMIT record** (sequence=42, generation=7):

```
plaintext_size = 8 + 2 + 2 + 4 + 40 = 56 bytes
encrypted_body = 56-byte plaintext + 16-byte tag = 72 bytes
record_len       = 4 + 72 = 76 bytes total

Layout:
  0x00  record_len   = 0x0000004C (76)
  0x04  ciphertext   = AEAD( seq=42, op=COMMIT(5), plen=40, gen=7, hash[32] )
  ...   tag          = last 16 bytes of encrypted_body
```

Golden vector: `tests/vectors/journal_commit_v1.bin`.

---

## Encrypted Volume Manifest (v1: flat sorted inode list)

v1 uses a **flat sorted inode list** (OQ-02 resolved). No B+ tree, no path-hash keys in v1.

### Manifest Pages

| `page_index` | `page_type` | Content |
|--------------|-------------|---------|
| 0 | `1` INODE_TABLE | Sorted inode entries |
| 1 | `2` FREE_BITMAP | Outer chunk allocation bitmap |
| 2+ | reserved | Future sharding (v1.1 multi-page inode table) |

Each page plaintext (32-byte page header + body) is max **65536** bytes before AEAD.

### v1 Inode Capacity Limit

```
max_inode_page_body = 65536 − 32 = 65504 bytes
max_inode_entries   = floor(65504 / 340) = 192
```

v1 supports at most **192 inode entries** (files + directories + `HIDDEN_RESERVATION`) per volume. Attempting to add inode #193 returns `ErrorCode::ManifestFull`. v1.1 will shard inode pages at `page_index ≥ 2`.

### Manifest Page Plaintext Layout

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `page_magic` | `SVMP` |
| 4 | 2 | `page_version` | `1` |
| 6 | 2 | `page_type` | `1`=inode, `2`=bitmap |
| 8 | 8 | `generation` | |
| 16 | 8 | `page_index` | |
| 24 | 4 | `entry_count` | Inode count or bitmap byte length |
| 28 | 4 | `reserved` | Zero |
| 32 | variable | `body` | See below |

**Page AEAD key:** `hkdf_manifest_page_key(VMK, generation, page_index)` — see `key-hierarchy.md`.

**Inode page body:** `entry_count` × inode entries, sorted ascending by `(parent_id, name_utf8)`. Lookup via binary search; optional in-memory hash cache on unlock.

**Bitmap page body:** Packed bits, 2 bits per chunk:

| Value | Meaning |
|-------|---------|
| `00` | Free |
| `01` | Outer allocated |
| `10` | **Hidden reserved** (set at `create-hidden`) |
| `11` | Reserved future |

Chunk index `i` → byte `i/4`, bits `(i%4)*2`.

### Inode Entry (340 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 2 | `inode_version` | `1` |
| 2 | 1 | `entry_type` | `1`=file, `2`=dir, `3`=symlink (v2), `4`=HIDDEN_RESERVATION |
| 3 | 1 | `name_len` | Max 255 |
| 4 | 256 | `name_utf8` | Zero-padded |
| 260 | 8 | `parent_id` | `0` = root |
| 268 | 8 | `inode_id` | Unique |
| 276 | 8 | `size` | File size |
| 284 | 8 | `mtime_unix` | |
| 292 | 4 | `chunk_count` | |
| 296 | 4 | `flags` | `READONLY`, `DELETED` |
| 300 | 8 | `first_chunk_id` | |
| 308 | 24 | `base_nonce` | Random 24-byte XChaCha base nonce per file |
| 332 | 8 | `data_generation` | Manifest generation used when chunk AEAD was written; `0` = legacy (read uses current manifest generation) |
| 340 | 0 | — | End of inode record |

**`HIDDEN_RESERVATION` entry:** `name_utf8` = `"__sv_hidden_reserve__"`, `parent_id=0`, `first_chunk_id=hidden_start_chunk`, `chunk_count=hidden_chunk_count`, `size=0`.

**Outer allocation rule:** Reject any allocation where chunk bit is `10` or chunk index ≥ `hidden_start_chunk` derived from reservation entry.

### `manifest_root_hash` Computation

Computed at `COMMIT` over **decrypted plaintext** of all manifest pages for the committed `generation`:

```
per_page_digest[i] = BLAKE2b-256(
    page_type_u16_le ||
    page_version_u16_le ||
    generation_u64_le ||
    page_index_u64_le ||
    entry_count_u32_le ||
    body_bytes
)

manifest_root_hash = BLAKE2b-256( per_page_digest[0] || per_page_digest[1] || ... )
```

Pages ordered by ascending `page_index`. v1 includes pages 0 (inode table) and 1 (bitmap). `body_bytes` is the raw page body (inode entries or bitmap bytes). Page header fields (`page_magic`, `reserved`) are **not** included in the hash — only the fields listed above plus body.

Golden vector: `tests/vectors/manifest_root_hash_v1.json`.

---

## Data Area — Chunk Layout

Per chunk on disk (no per-chunk nonce stored):

| Segment | Size |
|---------|------|
| `ciphertext` | `chunk_size` bytes |
| `tag` | 16 bytes |

**Chunk AEAD key:** `hkdf_chunk_key(VMK, chunk_id)`  
**Chunk nonce (24 bytes, derived):**

```
chunk_nonce = BLAKE2b-256(base_nonce || chunk_index_u64_le)[0:24]
```

**Associated data:** `inode_id_u64_le || chunk_index_u32_le || manifest_generation_u64_le` (16 bytes)

**Padding:** Final chunk PKCS#7-like (1–16 bytes).

---

## Hidden Volume Region

```
hidden_offset = data_offset + data_size - hidden_max_size   (outer header fields)
```

### Hidden Sub-Layout (relative to `hidden_offset`)

| Relative offset | Default size | Content |
|-----------------|--------------|---------|
| `hidden_journal_offset` (0) | `hidden_journal_size` (4 MiB) | Hidden journal |
| `hidden_manifest_offset` (4 MiB) | `hidden_manifest_size` (8 MiB) | Hidden manifest |
| `hidden_data_offset` (12 MiB) | remainder | Hidden file chunks |

Hidden header stores these offsets/sizes in fields `0x108–0x130`. Hidden manifest uses the same flat sorted inode format scoped to hidden data chunks.

### Outer Write Enforcement

| Check | When | Rule |
|-------|------|------|
| Bitmap | Always (outer unlock) | No alloc into `10` hidden-reserved chunks |
| Hard boundary | Always | `chunk_end ≤ hidden_offset` (in bytes) |
| Protection margin | `protect_hidden=true` | `write_end ≤ hidden_offset − protect_margin_bytes` |

`protect_margin_bytes` default: `max(134217728, data_size * 5 / 100)` (uint64).

---

## Versioning Rules

| Change | Action |
|--------|--------|
| Additive superblock field | Minor bump, backward compatible |
| New `kdf_type` | v1.1+ |
| New manifest page types | v1.1+ |

Readers reject unknown mandatory `format_version`.

---

## CRC32

Polynomial: IEEE 802.3 (`0xEDB88320` reflected). Detect-only; not a security control.

---

## Reference Constants

```cpp
constexpr uint8_t  SVLT_MAGIC[4] = {'S','V','L','T'};
constexpr uint16_t SVLT_FORMAT_VERSION = 1;
constexpr size_t   SVLT_SECTOR_SIZE = 512;
constexpr size_t   SVLT_HEADER_SIZE = 4096;
constexpr size_t   SVLT_HEADER_NONCE_SIZE = 24;  // crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
constexpr size_t   SVLT_AAD_SIZE = 80;
constexpr uint32_t SVLT_DEFAULT_CHUNK_SIZE = 1024 * 1024;
constexpr size_t   SVLT_INODE_SIZE = 340;
constexpr uint32_t SVLT_MAX_INODES_V1 = 192;
constexpr size_t   SVLT_SUPERBLOCK_SIZE = 512;
constexpr size_t   SVLT_SUPERBLOCK_RESERVED = 304;  // bytes at 0xD0
```

---

*Normative format specification for Shadow Vault v1 `.svlt` containers.*