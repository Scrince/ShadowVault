## @file
# Platform DSC for standalone ShadowVaultBoot.efi (lab / QEMU OVMF chain-load).
##
# Build (from EDK2 root, after `edksetup`):
#   build -p ShadowVaultPkg/ShadowVaultPkg.dsc -a X64 -t VS2022
# Or module-only:
#   build -p ShadowVaultPkg/ShadowVaultPkg.dsc -m ShadowVaultPkg/Application/ShadowVaultBoot/ShadowVaultBoot.inf -a X64 -t VS2022
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
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
  SvPbaEfiStub|ShadowVaultPkg/Library/SvPbaEfiStub/SvPbaEfiStub.inf
  SvPbaChainLoad|ShadowVaultPkg/Library/SvPbaChainLoad/SvPbaChainLoad.inf
  SvPbaHandoff|ShadowVaultPkg/Library/SvPbaHandoff/SvPbaHandoff.inf

[Components]
  ShadowVaultPkg/Application/ShadowVaultBoot/ShadowVaultBoot.inf
  ShadowVaultPkg/Application/LabBootManager/LabBootManager.inf
  ShadowVaultPkg/Application/LabRescue/LabRescue.inf