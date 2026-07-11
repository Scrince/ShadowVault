Shadow Vault — Portable (USB) Edition
=====================================

Copy this entire folder to a USB drive or any folder. No installer required.

Quick start
-----------
1. Open Command Prompt in this folder (or double-click sv.cmd).
2. Create a vault on the USB stick:
     svcli create vaults\myvault.svlt --size 64M --password "your-passphrase"
3. Store a file:
     svcli --vault vaults\myvault.svlt put .\myfile.txt /docs/myfile.txt --password "your-passphrase"
4. List contents:
     svcli --vault vaults\myvault.svlt list / --password "your-passphrase"
5. Extract a file:
     svcli --vault vaults\myvault.svlt get /docs/myfile.txt .\out.txt --password "your-passphrase"

Mount (optional — exposes vault as a drive letter)
--------------------------------------------------
Requires WinFsp (bundled as winfsp-x64.dll). From this folder:
     svcli mount vaults\myvault.svlt --point X: --password "your-passphrase"
Press Ctrl+C to dismount. Use --allow-host-integration if no_host_traces is enabled in shadowvault.ini.

Portable mode
-------------
Portable mode activates automatically because shadowvault.ini is beside svcli.exe.
Settings, temp files, and caches stay under this folder (see .shadowvault\).

Folders
-------
  vaults\     — put your .svlt containers here (recommended)
  keyfiles\   — optional keyfiles for two-factor unlock
  doc\        — license and CLI reference
  .shadowvault\ — temp/cache (created at runtime)

Security notes
--------------
- Use a strong passphrase; USB loss exposes the container file.
- Eject the drive only after locking: svcli lock
- For minimal host traces, edit shadowvault.ini and set no_host_traces=1
  (mount and some OS integrations will be blocked).

Version
-------
See: svcli --version