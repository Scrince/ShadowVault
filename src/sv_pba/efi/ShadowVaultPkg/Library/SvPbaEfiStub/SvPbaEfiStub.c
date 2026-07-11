/** @file
  Stub unlock implementation until freestanding sv_pba_core is linked.

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: MIT
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#include <sv_pba_efi.h>

STATIC
BOOLEAN
PassphraseEqualsAscii (
  IN CONST CHAR16  *PassphraseUtf16,
  IN CONST CHAR8   *Ascii
  )
{
  UINTN  Index;

  if ((PassphraseUtf16 == NULL) || (Ascii == NULL)) {
    return FALSE;
  }

  for (Index = 0; Ascii[Index] != '\0'; Index++) {
    if (PassphraseUtf16[Index] != (CHAR16)Ascii[Index]) {
      return FALSE;
    }
  }

  return PassphraseUtf16[Index] == L'\0';
}

EFI_STATUS
EFIAPI
SvPbaEfiTryUnlock (
  IN CONST CHAR16  *PassphraseUtf16
  )
{
  //
  // Lab scaffold: accept QEMU default test passphrase. Replaced by sv_pba_core
  // unlock_primary_header once freestanding crypto is extracted.
  //
  if (PassphraseEqualsAscii (PassphraseUtf16, "shadowvault-lab")) {
    return EFI_SUCCESS;
  }

  return EFI_ACCESS_DENIED;
}