/** @file
  Chain-load a UEFI PE image from the PBA ESP volume.

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: MIT
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Guid/FileInfo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#include <sv_pba_efi.h>

STATIC
EFI_STATUS
ReadEntireFileFromRoot (
  IN  EFI_FILE_PROTOCOL  *Root,
  IN  CONST CHAR16       *Path,
  OUT UINT8              **BufferOut,
  OUT UINTN              *SizeOut
  )
{
  EFI_STATUS         Status;
  EFI_FILE_PROTOCOL  *File;
  UINT64             FileSize;
  UINTN              ReadSize;
  UINT8              *Buffer;

  if ((Root == NULL) || (Path == NULL) || (BufferOut == NULL) || (SizeOut == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *BufferOut = NULL;
  *SizeOut   = 0;

  Status = Root->Open (Root, &File, (CHAR16 *)Path, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  {
    EFI_FILE_INFO  *Info;
    UINTN          InfoSize;

    InfoSize = 0;
    Status   = File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      File->Close (File);
      return EFI_NOT_FOUND;
    }

    Info = AllocatePool (InfoSize);
    if (Info == NULL) {
      File->Close (File);
      return EFI_OUT_OF_RESOURCES;
    }

    Status = File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, Info);
    if (EFI_ERROR (Status)) {
      FreePool (Info);
      File->Close (File);
      return Status;
    }

    FileSize = Info->FileSize;
    FreePool (Info);
  }

  if (FileSize == 0) {
    File->Close (File);
    return EFI_NOT_FOUND;
  }

  Buffer = AllocatePool ((UINTN)FileSize);
  if (Buffer == NULL) {
    File->Close (File);
    return EFI_OUT_OF_RESOURCES;
  }

  ReadSize = (UINTN)FileSize;
  Status   = File->Read (File, &ReadSize, Buffer);
  File->Close (File);
  if (EFI_ERROR (Status) || (ReadSize != (UINTN)FileSize)) {
    FreePool (Buffer);
    return EFI_DEVICE_ERROR;
  }

  *BufferOut = Buffer;
  *SizeOut   = ReadSize;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ChainLoadFromRoot (
  IN EFI_FILE_PROTOCOL  *Root,
  IN CONST CHAR16       *EfiPath
  )
{
  EFI_STATUS  Status;
  UINT8       *Buffer;
  UINTN       Size;
  EFI_HANDLE  ImageHandle;

  Status = ReadEntireFileFromRoot (Root, EfiPath, &Buffer, &Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ImageHandle = NULL;
  Status      = gBS->LoadImage (FALSE, gImageHandle, NULL, Buffer, Size, &ImageHandle);
  FreePool (Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Print (L"Chain-loading %s\r\n", EfiPath);
  return gBS->StartImage (ImageHandle, NULL, NULL);
}

STATIC
EFI_STATUS
ChainLoadFromDevice (
  IN EFI_HANDLE    DeviceHandle,
  IN CONST CHAR16  *EfiPath
  )
{
  EFI_STATUS                 Status;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePath;
  EFI_HANDLE                 ImageHandle;

  DevicePath = FileDevicePath (DeviceHandle, (CHAR16 *)EfiPath);
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ImageHandle = NULL;
  Status      = gBS->LoadImage (
                       FALSE,
                       gImageHandle,
                       DevicePath,
                       NULL,
                       0,
                       &ImageHandle
                       );
  FreePool (DevicePath);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Print (L"Chain-loading %s\r\n", EfiPath);
  Status = gBS->StartImage (ImageHandle, NULL, NULL);
  return Status;
}

EFI_STATUS
EFIAPI
SvPbaEfiChainLoadFromEsp (
  IN CONST CHAR16  *EfiPath
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;

  if ((EfiPath == NULL) || (EfiPath[0] == L'\0')) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return ChainLoadFromDevice (LoadedImage->DeviceHandle, EfiPath);
}

EFI_STATUS
EFIAPI
SvPbaEfiChainLoadBootManager (
  VOID
  )
{
  STATIC CONST CHAR16 *const kCandidates[] = {
    L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
    L"\\EFI\\BOOT\\BOOTX64.EFI",
    NULL
  };
  UINTN  Index;
  EFI_STATUS  Status;
  EFI_STATUS  LastStatus;

  LastStatus = EFI_NOT_FOUND;
  for (Index = 0; kCandidates[Index] != NULL; ++Index) {
    Status = SvPbaEfiChainLoadFromEsp (kCandidates[Index]);
    if (!EFI_ERROR (Status)) {
      return EFI_SUCCESS;
    }
    LastStatus = Status;
  }

  Print (L"No boot manager candidate found on ESP.\r\n");
  return LastStatus;
}

EFI_STATUS
EFIAPI
SvPbaEfiTryRescueUsb (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles;
  UINTN       Index;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    Print (L"Rescue: no file systems (%r)\r\n", Status);
    return Status;
  }

  for (Index = 0; Index < HandleCount; ++Index) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;
    EFI_FILE_PROTOCOL              *Root;
    EFI_FILE_PROTOCOL              *Rescue;

    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&FileSystem
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = FileSystem->OpenVolume (FileSystem, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = Root->Open (
                     Root,
                     &Rescue,
                     L"\\EFI\\ShadowVault\\Rescue\\ShadowVaultRescue.efi",
                     EFI_FILE_MODE_READ,
                     0
                     );
    if (!EFI_ERROR (Status)) {
      Rescue->Close (Rescue);
      FreePool (Handles);
      Print (L"Rescue media detected.\r\n");
      Status = ChainLoadFromRoot (
                 Root,
                 L"\\EFI\\ShadowVault\\Rescue\\ShadowVaultRescue.efi"
                 );
      Root->Close (Root);
      return Status;
    }
    Root->Close (Root);
  }

  FreePool (Handles);
  Print (L"Rescue: no ShadowVault rescue media found.\r\n");
  return EFI_NOT_FOUND;
}