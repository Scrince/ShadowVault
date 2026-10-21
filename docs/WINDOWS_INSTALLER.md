# Windows installer

Installs the Windows application and its configuration file.

Installer: `release/windows/ShadowVault-Setup-1.2.0.exe`

SHA-256: `17392c47a45aa355d24c7e64b7508c17833e679b0d2462d414707467ed85b3bf`

Detached OpenPGP signature: `release/windows/ShadowVault-Setup-1.2.0.exe.asc`

This is a detached OpenPGP release signature, not a Windows Authenticode signature.

Public key: `docs/ShadowVault_Release_Signing_2026_pubkey.asc`
Fingerprint: `8D255131790637D749973F59D0A6DC70DB715626`

Compare the fingerprint above before importing the key. From the project root:

```powershell
gpg --show-keys docs/ShadowVault_Release_Signing_2026_pubkey.asc
gpg --import docs/ShadowVault_Release_Signing_2026_pubkey.asc
gpg --verify release/windows/ShadowVault-Setup-1.2.0.exe.asc release/windows/ShadowVault-Setup-1.2.0.exe
gpg --verify release/windows/SHA256SUMS.asc release/windows/SHA256SUMS
```
