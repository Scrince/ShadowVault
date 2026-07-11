# Optional EDK2 build — requires EDK2_ROOT and BaseTools (not vendored).

if(NOT SV_BUILD_PBA_EFI)
    message(STATUS "sv_pba efi: skipped (SV_BUILD_PBA_EFI=OFF)")
    return()
endif()

if(NOT DEFINED EDK2_ROOT
   OR (NOT EXISTS "${EDK2_ROOT}/edksetup.bat" AND NOT EXISTS "${EDK2_ROOT}/edksetup.sh"))
    message(WARNING "sv_pba efi: EDK2_ROOT not set or invalid — ShadowVaultBoot.efi target unavailable")
    return()
endif()

if(CMAKE_HOST_WIN32)
    set(_sv_edk2_script "${CMAKE_CURRENT_SOURCE_DIR}/build-edk2.ps1")
    add_custom_target(ShadowVaultBoot_efi
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${_sv_edk2_script}"
            -Edk2Root "${EDK2_ROOT}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building ShadowVaultBoot.efi via EDK2"
        VERBATIM
    )
else()
    set(_sv_edk2_script "${CMAKE_CURRENT_SOURCE_DIR}/build-edk2.sh")
    add_custom_target(ShadowVaultBoot_efi
        COMMAND env EDK2_ROOT="${EDK2_ROOT}" bash "${_sv_edk2_script}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building ShadowVaultBoot.efi via EDK2"
        VERBATIM
    )
endif()

message(STATUS "sv_pba efi: ShadowVaultBoot_efi target registered (EDK2_ROOT=${EDK2_ROOT})")