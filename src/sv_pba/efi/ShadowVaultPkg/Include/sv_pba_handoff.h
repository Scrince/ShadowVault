/** @file
  VMK handoff blob for Phase 1 lab spike (OQ-S01).

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: GPL-3.0-or-later
**/

#ifndef SV_PBA_HANDOFF_H_
#define SV_PBA_HANDOFF_H_

#include <Uefi.h>

#define SV_PBA_VMK_HANDOFF_MAGIC    0x53565042U
#define SV_PBA_VMK_HANDOFF_VERSION  1U
#define SV_PBA_VMK_HANDOFF_SIZE     64U

#pragma pack(1)
typedef struct {
  UINT32    Magic;
  UINT32    Version;
  UINT8     Vmk[32];
  UINT8     Reserved[24];
} SV_PBA_VMK_HANDOFF;
#pragma pack()

#define SV_PBA_VMK_HANDOFF_VARIABLE_NAME  L"SvPbaVmkHandoff"

extern EFI_GUID  gSvPbaVmkHandoffGuid;

EFI_STATUS
EFIAPI
SvPbaEfiSealVmkHandoff (
  IN CONST UINT8  *Vmk,
  IN UINTN        VmkLen
  );

EFI_STATUS
EFIAPI
SvPbaEfiClearVmkHandoff (
  VOID
  );

#endif // SV_PBA_HANDOFF_H_