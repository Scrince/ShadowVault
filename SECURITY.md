# Security Policy

Shadow Vault handles encrypted user data and credentials. This policy states the supported security boundary, practical operating guidance, and how to report a vulnerability. It describes the current implementation; it does not certify the software or guarantee that every risk is eliminated.

## Security status and scope

The repository currently identifies itself as version 1.2.0. The repository does not publish a formal long-term-support matrix or an independent security audit/certification report, and it does not establish a reproducible-build guarantee. Treat the exact source commit, build configuration, operating system, and optional components as material to any security claim. The PBA and system-encryption work is experimental/design-stage and must not be treated as a production security feature.

> **Critical unresolved cryptographic risk:** In the inspected source, replacing an existing file preserves its inode `base_nonce`, while chunk keys are derived from physical chunk IDs and chunk nonces from `base_nonce` plus logical chunk index. The allocator can later recycle those IDs. This appears to permit reuse of the same AEAD key/nonce pair for different plaintext; the manifest generation is AAD and does not prevent that failure. This undermines confidentiality and authentication for affected rewritten chunks. Treat this implementation as unsuitable for protecting sensitive data that is rewritten until nonce uniqueness is corrected and regression-tested. See [TM-15 in the threat model](THREAT_MODEL.md#6-threat-analysis).

In-scope security reports include:

- Unauthorized decryption, credential bypass, key/nonce misuse, or failure to authenticate protected data.
- Memory-safety or parser flaws reachable through a vault, header, keyfile, or command-line input.
- Data loss or hidden-region overwrite caused by a supported application operation.
- Credential/plaintext disclosure through application-managed files, logs, command handling, GUI workflows, or mount behavior.
- A security-relevant failure in recovery, verification, key rotation, or access-control behavior.
- A compromised dependency, build, signing, or release process affecting Shadow Vault artifacts.

Please distinguish product behavior from an endpoint compromise. Shadow Vault cannot protect secrets from an operating system or privileged process that controls the machine while the vault is unlocked.

## Reporting a vulnerability

**Do not publish an exploitable vulnerability in a public issue, discussion, pull request, or commit before maintainers have had an opportunity to assess and mitigate it.** Do not include passwords, private keys, personal data, or an actual user's vault/container in a report.

Preferred channel: use GitHub's **Report a vulnerability** / private security advisory flow for [Scrince/ShadowVault](https://github.com/Scrince/ShadowVault/security/advisories/new) if GitHub makes that option available. If private reporting is unavailable, contact the repository maintainer privately through the [Scrince GitHub profile](https://github.com/Scrince) and request a secure channel. Do not assume that a public issue is confidential.

Include, where safe:

1. A concise description of the violated security property and its practical impact.
2. The affected version/commit, operating system, compiler, build flags, and optional components (GUI, mount, portable, PBA).
3. Minimal reproduction steps and, if necessary, a proof of concept that does not target other users or destroy data.
4. Any relevant logs with passwords, keyfile contents, user paths, and container data removed.
5. Whether the issue is known to be exploited or publicly disclosed, and any proposed mitigation.

The maintainers will acknowledge and investigate reports as capacity permits. There is no published response-time SLA, bounty program, or guaranteed release schedule. Coordinated disclosure timing should be agreed with the reporter after triage; urgent exploitation may require faster protective disclosure.

## Safe operating guidance

### Credentials

- Prefer a unique, high-entropy passphrase. The engine's 12-byte minimum is only a syntactic floor, not evidence of strength. Avoid reusing account passwords.
- Use the interactive CLI prompt for normal use. Inline `--password` or `--new-password` arguments may be visible in shell history and process listings. Environment variables and password files also require careful protection.
- Protect keyfiles as secrets. Keep independent, access-controlled backups; do not store the only keyfile beside the only vault copy.
- Password changes rewrap the vault header; they do not erase old copies of the container or old header backups. Any retained backup made under an earlier credential may preserve access using that earlier credential. Review and securely retire backups according to your storage system's capabilities.
- Do not share `.svhdr` header backups publicly. They contain encrypted header material and salts that permit offline credential guessing.

### Host and plaintext handling

- Use a patched, trusted operating system and keep endpoint protection and account isolation appropriate to the data.
- Use full-disk encryption on the host if local storage, swap/pagefile, hibernation, crash dumps, or temporary output may contain sensitive material.
- Treat `get` output, GUI exports, and files opened through a mount as ordinary plaintext files governed by host permissions, backups, indexing, and deletion behavior.
- `SecureBuffer` attempts to lock selected buffers and wipes them on release, but OS memory-lock calls can fail and most plaintext containers/framework copies are not comprehensively locked or wiped. Locking the vault is not equivalent to rebooting a compromised host or erasing all prior memory copies.
- Keep independent backups of the encrypted container and test restoration on copies. `verify` checks structural/authentication properties; it cannot prove freshness or replace a backup.

### Hidden volumes and portable mode

- Do not rely on hidden volumes for plausible deniability. The current implementation sets a cleartext hidden-configured superblock flag when creating a hidden volume; file-format and usage patterns can also be analyzed. Separate credentials and encrypted regions do not conceal the fact of use.
- Enable hidden-volume protection for outer-volume writes when working with a container that has hidden data. GUI outer unlock defaults the control on; CLI and mount workflows require deliberate option selection. Do not use a raw disk editor or another writer against an open container.
- Portable and “no host traces” settings suppress only selected application-managed persistence. They do not erase existing artifacts or control OS recent-item lists, shell history, swap/pagefile, crash reports, filesystem metadata, endpoint telemetry, backups, or other applications.
- Mount support is optional and experimental. It adds a filesystem driver/kernel boundary and exposes plaintext to the host filesystem client. Use read-only mounts where appropriate and evaluate the platform driver separately.

### Integrity, recovery, and release artifacts

- Run verification before and after high-impact recovery operations. Make a byte-for-byte backup first; perform repair on a working copy whenever possible.
- Keep header backups encrypted and access-controlled. Restoring an old header can restore old credential state or overwrite newer key metadata.
- CRCs in structural metadata detect certain accidental changes but are not cryptographic authentication. Authenticated regions still do not prevent replay of an older complete valid container.
- Verify release signatures and checksums using a signing key/fingerprint obtained through a trusted, independent channel. A checksum downloaded beside an artifact does not by itself establish authenticity.
- Build dependencies may be fetched if vendored sources are absent. Review dependency revisions, build scripts, compiler/toolchain provenance, and the produced artifact; source availability alone does not prove the binary corresponds to reviewed code.

## Security architecture at a glance

| Layer | Current design | Boundary to preserve |
|---|---|---|
| Credentials | Password bytes and optional keyfile hashes feed a credential combiner, then Argon2id derives header key material. | Never log credential material; make clear that a low-entropy password remains guessable offline. |
| Header | XChaCha20-Poly1305-IETF authenticated encrypted headers wrap the volume master key. | Cleartext superblock salts, KDF fields, layout, checksums, flags, and fixed sector placement are not secret. |
| Manifest/journal | VMK-derived keys protect manifest pages and journal records; a root hash/generation links manifest state. | CRCs are not MACs; no external rollback counter exists. |
| File chunks | VMK-derived per-chunk keys, derived nonces, associated context, and AEAD tags protect stored chunks. | Plaintext returned to callers and exported to host paths is outside the at-rest boundary. |
| Memory | Selected key buffers use best-effort OS page locking and explicit wiping. | The OS remains trusted; ordinary strings/vectors and GUI/framework copies can retain secrets. |
| Portable settings | Selected recent-path/file-log behavior is disabled or redirected. | This is not a forensic erasure feature or an OS-wide trace policy. |

For the detailed adversary model, data-flow boundaries, threat scenarios, and implementation limitations, see [`THREAT_MODEL.md`](THREAT_MODEL.md). The file-format and key-hierarchy design notes are useful references, but implementation behavior governs where they differ.
