/** @file
  Lab stand-in for ShadowVaultRescue.efi on rescue USB media.

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: GPL-3.0-or-later
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gImageHandle = ImageHandle;
  gST          = SystemTable;
  gBS          = SystemTable->BootServices;

  Print (L"LAB_RESCUE_OK\r\n");
  gBS->Stall (500 * 1000);
  return EFI_SUCCESS;
}