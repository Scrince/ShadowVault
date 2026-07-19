

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#ifdef SV_PBA_EFI_LAB_SERIAL
#include <Library/IoLib.h>

#define SV_PBA_COM1_BASE  0x3F8U
#define SV_PBA_COM1_LSR   (SV_PBA_COM1_BASE + 5U)
#endif

#include <sv_pba_efi.h>
#include <sv_pba_handoff.h>

#define SV_PBA_MAX_PASSPHRASE_CHARS  128

#ifdef SV_PBA_EFI_LAB_SERIAL
#define SV_PBA_SERIAL_POLL_US  10000U
#endif

typedef enum {
  SvPbaMenuBoot = 1,
  SvPbaMenuRescue = 2,
} SV_PBA_MENU_CHOICE;

STATIC
EFI_STATUS
OpenVolumeOnLoadedImageForLab (
  OUT EFI_FILE_PROTOCOL  **Root
  )
{
  EFI_STATUS                       Status;
  EFI_LOADED_IMAGE_PROTOCOL        *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;

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
ReadPassphrase (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferChars
  )
{
  EFI_STATUS    Status;
  EFI_INPUT_KEY Key;
  UINTN         Index;

  if ((Buffer == NULL) || (BufferChars < 2)) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer[0] = L'\0';
  Index     = 0;

  while (TRUE) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (EFI_ERROR (Status)) {
      gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
      continue;
    }

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Buffer[Index] = L'\0';
      Print (L"\r\n");
      return EFI_SUCCESS;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Index > 0) {
        Index--;
        Buffer[Index] = L'\0';
        Print (L"\b \b");
      }
      continue;
    }

    if ((Key.UnicodeChar >= 0x20) && (Key.UnicodeChar < 0x7F) && (Index + 1 < BufferChars)) {
      Buffer[Index++] = Key.UnicodeChar;
      Buffer[Index]   = L'\0';
      Print (L"*");
    }
  }
}

#ifdef SV_PBA_EFI_LAB_SERIAL
STATIC
EFI_STATUS
ReadPassphraseSerial (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferChars
  )
{
  UINTN  Index;
  UINT8  Byte;

  if ((Buffer == NULL) || (BufferChars < 2)) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer[0] = L'\0';
  Index     = 0;

  while (Index + 1 < BufferChars) {
    if ((IoRead8 (SV_PBA_COM1_LSR) & 0x01U) == 0U) {
      gBS->Stall (SV_PBA_SERIAL_POLL_US);
      continue;
    }

    Byte = IoRead8 (SV_PBA_COM1_BASE);

    if ((Byte == '\r') || (Byte == '\n')) {
      if (Index == 0) {
        continue;
      }
      Buffer[Index] = L'\0';
      Print (L"\r\n");
      return EFI_SUCCESS;
    }

    if ((Byte >= 0x20) && (Byte < 0x7F)) {
      Buffer[Index++] = (CHAR16)Byte;
      Buffer[Index]   = L'\0';
      Print (L"*");
    }
  }

  return EFI_BUFFER_TOO_SMALL;
}
#endif

STATIC
EFI_STATUS
ReadPassphraseInteractive (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferChars
  )
{
#ifdef SV_PBA_EFI_LAB_SERIAL
  Print (L"[lab serial input]\r\n");
  return ReadPassphraseSerial (Buffer, BufferChars);
#else
  return ReadPassphrase (Buffer, BufferChars);
#endif
}

#ifdef SV_PBA_EFI_LAB_SERIAL
STATIC
EFI_STATUS
ReadMenuChoiceSerial (
  OUT SV_PBA_MENU_CHOICE  *Choice
  )
{
  UINT8  Byte;

  if (Choice == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    if ((IoRead8 (SV_PBA_COM1_LSR) & 0x01U) == 0U) {
      gBS->Stall (SV_PBA_SERIAL_POLL_US);
      continue;
    }

    Byte = IoRead8 (SV_PBA_COM1_BASE);
    if ((Byte == '\r') || (Byte == '\n')) {
      continue;
    }

    if (Byte == '1') {
      *Choice = SvPbaMenuBoot;
      Print (L"\r\n");
      return EFI_SUCCESS;
    }
    if (Byte == '2') {
      *Choice = SvPbaMenuRescue;
      Print (L"\r\n");
      return EFI_SUCCESS;
    }
  }
}
#endif

STATIC
EFI_STATUS
ReadMenuChoiceConsole (
  OUT SV_PBA_MENU_CHOICE  *Choice
  )
{
  EFI_STATUS    Status;
  EFI_INPUT_KEY Key;
  UINTN         Index;

  if (Choice == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (EFI_ERROR (Status)) {
      gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
      continue;
    }

    if (Key.UnicodeChar == L'1') {
      *Choice = SvPbaMenuBoot;
      Print (L"\r\n");
      return EFI_SUCCESS;
    }
    if (Key.UnicodeChar == L'2') {
      *Choice = SvPbaMenuRescue;
      Print (L"\r\n");
      return EFI_SUCCESS;
    }
  }
}

STATIC
EFI_STATUS
ReadMenuChoice (
  OUT SV_PBA_MENU_CHOICE  *Choice
  )
{
#ifdef SV_PBA_EFI_LAB_SERIAL
  Print (L"[lab serial menu — 1=boot 2=rescue]\r\n");
  return ReadMenuChoiceSerial (Choice);
#else
  return ReadMenuChoiceConsole (Choice);
#endif
}

STATIC
VOID
WipePassphrase (
  IN OUT CHAR16  *Passphrase,
  IN     UINTN   CapacityChars
  )
{
  if (Passphrase != NULL) {
    SetMem (Passphrase, CapacityChars * sizeof (CHAR16), 0);
  }
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  CHAR16              Passphrase[SV_PBA_MAX_PASSPHRASE_CHARS];
  CHAR16              Banner[128];
  SV_PBA_MENU_CHOICE  MenuChoice;

  gImageHandle = ImageHandle;
  gST          = SystemTable;
  gBS          = SystemTable->BootServices;

  UnicodeSPrint (
    Banner,
    sizeof (Banner),
    L"ShadowVault PBA %s\r\n",
    SV_PBA_EFI_VERSION
    );
  Print (Banner);

#ifdef SV_PBA_EFI_LAB_AUTO_BOOT
  {
    EFI_FILE_PROTOCOL  *Root;
    EFI_FILE_PROTOCOL  *Marker;

    MenuChoice = SvPbaMenuBoot;
    if (!EFI_ERROR (OpenVolumeOnLoadedImageForLab (&Root))) {
      if (!EFI_ERROR (Root->Open (
                                Root,
                                &Marker,
                                L"\\EFI\\ShadowVault\\lab\\force_rescue_test",
                                EFI_FILE_MODE_READ,
                                0
                                ))) {
        Marker->Close (Marker);
        MenuChoice = SvPbaMenuRescue;
        Print (L"[lab auto-rescue]\r\n");
      } else {
        Print (L"[lab auto-boot]\r\n");
      }
      Root->Close (Root);
    } else {
      Print (L"[lab auto-boot]\r\n");
    }
  }
#else
  Print (L"[1] Unlock and boot\r\n");
  Print (L"[2] Recovery (USB detect)\r\n");
  Print (L"Choice: ");

  Status = ReadMenuChoice (&MenuChoice);
  if (EFI_ERROR (Status)) {
    return Status;
  }
#endif

  if (MenuChoice == SvPbaMenuRescue) {
    Status = SvPbaEfiTryRescueUsb ();
    gBS->Stall (3 * 1000 * 1000);
    return Status;
  }

  SetMem (Passphrase, sizeof (Passphrase), 0);

#if defined(SV_PBA_EFI_LAB_AUTO_BOOT) && defined(SV_PBA_EFI_LAB_SERIAL)
  {
    CONST CHAR8         *LabAscii = "shadowvault-lab";
    EFI_FILE_PROTOCOL   *Root;
    EFI_FILE_PROTOCOL   *Marker;
    UINTN               Index;

    if (!EFI_ERROR (OpenVolumeOnLoadedImageForLab (&Root))) {
      if (!EFI_ERROR (Root->Open (
                                Root,
                                &Marker,
                                L"\\EFI\\ShadowVault\\lab\\force_wrong_password",
                                EFI_FILE_MODE_READ,
                                0
                                ))) {
        Marker->Close (Marker);
        LabAscii = "not-the-lab-password";
        Print (L"Passphrase: [lab wrong-password test]\r\n");
      } else {
        Print (L"Passphrase: [lab fixed]\r\n");
      }
      Root->Close (Root);
    } else {
      Print (L"Passphrase: [lab fixed]\r\n");
    }
    for (Index = 0; LabAscii[Index] != '\0'; ++Index) {
      Passphrase[Index] = (CHAR16)LabAscii[Index];
    }
    Passphrase[Index] = L'\0';
    Status = EFI_SUCCESS;
  }
#else
  Print (L"Passphrase: ");
  Status = ReadPassphraseInteractive (Passphrase, SV_PBA_MAX_PASSPHRASE_CHARS);
  if (EFI_ERROR (Status)) {
    WipePassphrase (Passphrase, SV_PBA_MAX_PASSPHRASE_CHARS);
    Print (L"Input error: %r\r\n", Status);
    return Status;
  }
#endif

  Status = SvPbaEfiTryUnlock (Passphrase);
  WipePassphrase (Passphrase, SV_PBA_MAX_PASSPHRASE_CHARS);
  if (EFI_ERROR (Status)) {
    Print (L"Unlock failed.\r\n");
    return EFI_ACCESS_DENIED;
  }

#ifdef SV_PBA_EFI_FREESTANDING
  Print (L"Freestanding unlock OK.\r\n");
#else
  Print (L"Stub unlock OK.\r\n");
#endif
  Print (L"VMK handoff sealed (BS variable).\r\n");

  Status = SvPbaEfiChainLoadBootManager ();
  if (EFI_ERROR (Status)) {
    (VOID)SvPbaEfiClearVmkHandoff ();
    Print (L"Chain-load failed.\r\n");
    gBS->Stall (3 * 1000 * 1000);
  }

  return Status;
}