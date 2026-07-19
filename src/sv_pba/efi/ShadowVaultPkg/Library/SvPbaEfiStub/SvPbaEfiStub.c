

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
  
  
  if (PassphraseEqualsAscii (PassphraseUtf16, "shadowvault-lab")) {
    return EFI_SUCCESS;
  }

  return EFI_ACCESS_DENIED;
}