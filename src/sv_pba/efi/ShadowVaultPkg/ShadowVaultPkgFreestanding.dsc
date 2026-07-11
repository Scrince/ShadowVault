## @file
# ShadowVaultPkg with freestanding unlock (requires LIBSODIUM_ROOT / LIBSODIUM_INCLUDE).
##

[Defines]
  PLATFORM_NAME                  = ShadowVaultPkg
  PLATFORM_GUID                  = 8A1C5E32-4F7B-4D9A-B2E1-6C3D4E5F7081
  PLATFORM_VERSION               = 1.0
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/ShadowVaultPkg
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
  SvLibsodium|ShadowVaultPkg/Library/SvLibsodium/SvLibsodium.inf
  SvPbaFreestanding|ShadowVaultPkg/Library/SvPbaFreestanding/SvPbaFreestanding.inf
  SvPbaEfiUnlock|ShadowVaultPkg/Library/SvPbaEfiUnlock/SvPbaEfiUnlock.inf
  SvPbaChainLoad|ShadowVaultPkg/Library/SvPbaChainLoad/SvPbaChainLoad.inf
  SvPbaHandoff|ShadowVaultPkg/Library/SvPbaHandoff/SvPbaHandoff.inf

[Components]
  ShadowVaultPkg/Application/ShadowVaultBoot/ShadowVaultBootLab.inf
  ShadowVaultPkg/Application/LabBootManager/LabBootManager.inf
  ShadowVaultPkg/Application/LabRescue/LabRescue.inf