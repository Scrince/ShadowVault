/** @file
  EFI-facing bridge for sv_pba_core.

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: MIT
**/

#ifndef SV_PBA_EFI_H_
#define SV_PBA_EFI_H_

#include <Uefi.h>

#define SV_PBA_EFI_VERSION  L"0.2.0"

/**
  Attempt unlock using passphrase bytes (UTF-16 ASCII subset).

  @retval EFI_SUCCESS           Unlock accepted.
  @retval EFI_ACCESS_DENIED     Wrong credentials.
**/
EFI_STATUS
EFIAPI
SvPbaEfiTryUnlock (
  IN CONST CHAR16  *PassphraseUtf16
  );

/** Load \\EFI\\ShadowVault\\lab\\{header,superblock}.bin and run freestanding unlock. */
EFI_STATUS
EFIAPI
SvPbaEfiUnlockFromLab (
  IN CONST CHAR16  *PassphraseUtf16
  );

/** Chain-load a PE image path on the PBA ESP (e.g. bootmgfw.efi). */
EFI_STATUS
EFIAPI
SvPbaEfiChainLoadFromEsp (
  IN CONST CHAR16  *EfiPath
  );

/** Try bootmgfw.efi then BOOTX64.EFI on the ESP. */
EFI_STATUS
EFIAPI
SvPbaEfiChainLoadBootManager (
  VOID
  );

/** Sprint D rescue stub — detect rescue USB layout (no chain-load in Phase 1). */
EFI_STATUS
EFIAPI
SvPbaEfiTryRescueUsb (
  VOID
  );

#endif // SV_PBA_EFI_H_