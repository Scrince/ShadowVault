add_subdirectory(sv_common)
add_subdirectory(sv_platform)
add_subdirectory(sv_crypto)
add_subdirectory(sv_container)
add_subdirectory(sv_vault)
add_subdirectory(sv_recovery)
add_subdirectory(sv_keyfile)
add_subdirectory(sv_portable)

if(SV_BUILD_MOUNT)
    add_subdirectory(sv_mount)
endif()

if(SV_BUILD_PBA)
    add_subdirectory(sv_pba)
endif()

if(SV_BUILD_CLI)
    add_subdirectory(sv_cli)
endif()

if(SV_BUILD_GUI)
    add_subdirectory(sv_gui)
endif()