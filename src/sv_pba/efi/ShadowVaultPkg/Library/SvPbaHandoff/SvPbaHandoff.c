

#include <sv_pba_handoff.h>

#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_GUID  gSvPbaVmkHandoffGuid = {
  0x9E4F8A21, 0x6C3B, 0x4D2E, { 0xA1, 0xF8, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAB }
};

EFI_STATUS
EFIAPI
SvPbaEfiClearVmkHandoff (
  VOID
  )
{
  if (gRT == NULL) {
    return EFI_NOT_READY;
  }

  return gRT->SetVariable (
                SV_PBA_VMK_HANDOFF_VARIABLE_NAME,
                &gSvPbaVmkHandoffGuid,
                EFI_VARIABLE_BOOTSERVICE_ACCESS,
                0,
                NULL
                );
}

EFI_STATUS
EFIAPI
SvPbaEfiSealVmkHandoff (
  IN CONST UINT8  *Vmk,
  IN UINTN        VmkLen
  )
{
  SV_PBA_VMK_HANDOFF  Handoff;
  EFI_STATUS          Status;

  if ((Vmk == NULL) || (VmkLen != 32)) {
    return EFI_INVALID_PARAMETER;
  }
  if (gRT == NULL) {
    return EFI_NOT_READY;
  }

  (VOID)SvPbaEfiClearVmkHandoff ();

  SetMem (&Handoff, sizeof (Handoff), 0);
  Handoff.Magic   = SV_PBA_VMK_HANDOFF_MAGIC;
  Handoff.Version = SV_PBA_VMK_HANDOFF_VERSION;
  CopyMem (Handoff.Vmk, Vmk, 32);

  Status = gRT->SetVariable (
                    SV_PBA_VMK_HANDOFF_VARIABLE_NAME,
                    &gSvPbaVmkHandoffGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS,
                    sizeof (Handoff),
                    &Handoff
                    );
  SetMem (&Handoff, sizeof (Handoff), 0);
  return Status;
}