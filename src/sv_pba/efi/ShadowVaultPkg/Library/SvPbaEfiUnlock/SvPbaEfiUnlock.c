

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

#include <sv/pba/pba_unlock_fs.h>
#include <sv_pba_efi.h>
#include <sv_pba_handoff.h>

#define SV_PBA_LAB_HEADER_PATH     L"\\EFI\\ShadowVault\\lab\\header.bin"
#define SV_PBA_LAB_SUPERBLOCK_PATH L"\\EFI\\ShadowVault\\lab\\superblock.bin"
#define SV_PBA_FS_HEADER_SIZE      4096U
#define SV_PBA_FS_SUPERBLOCK_SIZE  512U
#define SV_PBA_MAX_PASSPHRASE_UTF8 128U

STATIC
EFI_STATUS
Utf16AsciiToUtf8 (
  IN  CONST CHAR16  *Utf16,
  OUT CHAR8         *Utf8,
  IN  UINTN         Utf8Capacity
  )
{
  UINTN  Index;

  if ((Utf16 == NULL) || (Utf8 == NULL) || (Utf8Capacity == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Utf16[Index] != L'\0'; ++Index) {
    if (Index + 1 >= Utf8Capacity) {
      return EFI_BUFFER_TOO_SMALL;
    }
    if (Utf16[Index] > 0x7FU) {
      return EFI_UNSUPPORTED;
    }
    Utf8[Index] = (CHAR8)Utf16[Index];
  }

  if (Index + 1 > Utf8Capacity) {
    return EFI_BUFFER_TOO_SMALL;
  }

  Utf8[Index] = '\0';
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MapPbaStatus (
  IN enum sv_pba_status  Status
  )
{
  switch (Status) {
  case SV_PBA_STATUS_OK:
    return EFI_SUCCESS;
  case SV_PBA_STATUS_WRONG_CREDENTIALS:
    return EFI_ACCESS_DENIED;
  case SV_PBA_STATUS_INVALID_ARGUMENT:
    return EFI_INVALID_PARAMETER;
  case SV_PBA_STATUS_CORRUPT_CONTAINER:
    return EFI_VOLUME_CORRUPTED;
  case SV_PBA_STATUS_PLATFORM_ERROR:
  default:
    return EFI_DEVICE_ERROR;
  }
}

STATIC
EFI_STATUS
OpenVolumeOnLoadedImage (
  OUT EFI_FILE_PROTOCOL  **Root
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;

  if (Root == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Root = NULL;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->HandleProtocol (
                  LoadedImage->DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&FileSystem
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return FileSystem->OpenVolume (FileSystem, Root);
}

STATIC
EFI_STATUS
ReadEntireFile (
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

EFI_STATUS
EFIAPI
SvPbaEfiUnlockFromLab (
  IN CONST CHAR16  *PassphraseUtf16
  )
{
  EFI_STATUS              Status;
  EFI_FILE_PROTOCOL       *Root;
  UINT8                   *Header;
  UINTN                   HeaderSize;
  UINT8                   *Superblock;
  UINTN                   SuperblockSize;
  CHAR8                   PassphraseUtf8[SV_PBA_MAX_PASSPHRASE_UTF8];
  sv_pba_unlock_request_t Request;
  UINT8                   Vmk[32];
  enum sv_pba_status      PbaStatus;

  Status = OpenVolumeOnLoadedImage (&Root);
  if (EFI_ERROR (Status)) {
    Print (L"ESP volume open failed: %r\r\n", Status);
    return Status;
  }

  Status = ReadEntireFile (Root, SV_PBA_LAB_HEADER_PATH, &Header, &HeaderSize);
  if (EFI_ERROR (Status)) {
    Print (L"Missing %s (%r)\r\n", SV_PBA_LAB_HEADER_PATH, Status);
    return Status;
  }

  Status = ReadEntireFile (Root, SV_PBA_LAB_SUPERBLOCK_PATH, &Superblock, &SuperblockSize);
  if (EFI_ERROR (Status)) {
    FreePool (Header);
    Print (L"Missing %s (%r)\r\n", SV_PBA_LAB_SUPERBLOCK_PATH, Status);
    return Status;
  }

  if ((HeaderSize < SV_PBA_FS_HEADER_SIZE) || (SuperblockSize < SV_PBA_FS_SUPERBLOCK_SIZE)) {
    Print (
      L"Lab vector size mismatch (header=%u super=%u)\r\n",
      (UINT32)HeaderSize,
      (UINT32)SuperblockSize
      );
    FreePool (Header);
    FreePool (Superblock);
    return EFI_VOLUME_CORRUPTED;
  }

  Status = Utf16AsciiToUtf8 (PassphraseUtf16, PassphraseUtf8, sizeof (PassphraseUtf8));
  if (EFI_ERROR (Status)) {
    FreePool (Header);
    FreePool (Superblock);
    return Status;
  }

  Request.primary_header_sector     = Header;
  Request.primary_header_sector_len = HeaderSize;
  Request.superblock_bytes          = Superblock;
  Request.superblock_bytes_len      = SuperblockSize;
  Request.password_utf8             = (const uint8_t *)PassphraseUtf8;
  Request.password_utf8_len         = AsciiStrLen (PassphraseUtf8);

  PbaStatus = sv_pba_unlock_primary_header (&Request, Vmk);
  Status    = MapPbaStatus (PbaStatus);
  if (!EFI_ERROR (Status)) {
    Status = SvPbaEfiSealVmkHandoff (Vmk, sizeof (Vmk));
  }

  SetMem (PassphraseUtf8, sizeof (PassphraseUtf8), 0);
  SetMem (Vmk, sizeof (Vmk), 0);
  FreePool (Header);
  FreePool (Superblock);

  return Status;
}

#ifdef SV_PBA_EFI_FREESTANDING
EFI_STATUS
EFIAPI
SvPbaEfiTryUnlock (
  IN CONST CHAR16  *PassphraseUtf16
  )
{
  return SvPbaEfiUnlockFromLab (PassphraseUtf16);
}
#endif