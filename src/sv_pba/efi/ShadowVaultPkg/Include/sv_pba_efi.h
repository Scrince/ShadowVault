

#ifndef SV_PBA_EFI_H_
#define SV_PBA_EFI_H_

#include <Uefi.h>

#define SV_PBA_EFI_VERSION  L"0.2.0"


EFI_STATUS
EFIAPI
SvPbaEfiTryUnlock (
  IN CONST CHAR16  *PassphraseUtf16
  );


EFI_STATUS
EFIAPI
SvPbaEfiUnlockFromLab (
  IN CONST CHAR16  *PassphraseUtf16
  );


EFI_STATUS
EFIAPI
SvPbaEfiChainLoadFromEsp (
  IN CONST CHAR16  *EfiPath
  );


EFI_STATUS
EFIAPI
SvPbaEfiChainLoadBootManager (
  VOID
  );


EFI_STATUS
EFIAPI
SvPbaEfiTryRescueUsb (
  VOID
  );

#endif 