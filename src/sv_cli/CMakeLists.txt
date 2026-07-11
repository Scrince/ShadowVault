set(SV_CLI_COMMAND_SOURCES
    commands/benchmark.cpp
    commands/change_password.cpp
    commands/create.cpp
    commands/create_hidden.cpp
    commands/dismount.cpp
    commands/get.cpp
    commands/header.cpp
    commands/keyfile.cpp
    commands/list.cpp
    commands/lock.cpp
    commands/mkdir.cpp
    commands/mount.cpp
    commands/portable_status.cpp
    commands/put.cpp
    commands/remove.cpp
    commands/rename.cpp
    commands/repair.cpp
    commands/unlock.cpp
    commands/verify.cpp
)

add_executable(sv_cli
    main.cpp
    cli_args.cpp
    command_registry.cpp
    output.cpp
    vault_cmd.cpp
    ${SV_CLI_COMMAND_SOURCES}
)

if(SV_SINGLE_EXE_PORTABLE)
    set_target_properties(sv_cli PROPERTIES OUTPUT_NAME ShadowVault)
else()
    set_target_properties(sv_cli PROPERTIES OUTPUT_NAME svcli)
endif()

target_include_directories(sv_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/generated
        ${CMAKE_SOURCE_DIR}/src/sv_vault/include
        ${CMAKE_SOURCE_DIR}/src/sv_container/include
        ${CMAKE_SOURCE_DIR}/src/sv_keyfile/include
        ${CMAKE_SOURCE_DIR}/src/sv_crypto/include
        ${CMAKE_SOURCE_DIR}/src/sv_recovery/include
)

target_link_libraries(sv_cli PRIVATE
    sv_common
    sv_platform
    sv_portable
    sv_vault
    sv_container
    sv_keyfile
    sv_recovery
    sv_crypto
)

if(SV_SINGLE_EXE_PORTABLE AND WIN32)
    if(NOT SV_WINFSP_RUNTIME_DLL OR NOT EXISTS "${SV_WINFSP_RUNTIME_DLL}")
        message(FATAL_ERROR "SV_SINGLE_EXE_PORTABLE requires SV_WINFSP_RUNTIME_DLL (WinFsp bin/winfsp-x64.dll)")
    endif()
    set(_sv_portable_ini "${CMAKE_SOURCE_DIR}/packaging/portable/shadowvault.ini")
    if(NOT EXISTS "${_sv_portable_ini}")
        message(FATAL_ERROR "Missing ${_sv_portable_ini}")
    endif()
    set(SV_EMBED_WINFSP_DLL "${SV_WINFSP_RUNTIME_DLL}")
    set(SV_EMBED_PORTABLE_INI "${_sv_portable_ini}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/single-exe/sv_embedded.rc.in"
        "${CMAKE_BINARY_DIR}/generated/sv_embedded.rc"
        @ONLY
    )
    enable_language(RC)
    target_sources(sv_cli PRIVATE "${CMAKE_BINARY_DIR}/generated/sv_embedded.rc")
    target_compile_definitions(sv_cli PRIVATE SV_EMBED_WINFSP=1)
    if(SV_BUILD_MOUNT)
        target_compile_definitions(sv_cli PRIVATE SV_HAVE_MOUNT=1)
    endif()
    target_link_libraries(sv_cli PRIVATE sv_mount)
    if(MSVC)
        target_link_options(sv_cli PRIVATE "/DELAYLOAD:winfsp-x64.dll")
        target_link_libraries(sv_cli PRIVATE delayimp.lib)
    endif()
elseif(SV_BUILD_MOUNT)
    target_link_libraries(sv_cli PRIVATE sv_mount)
    target_compile_definitions(sv_cli PRIVATE SV_HAVE_MOUNT=1)
    if(NOT SV_WINFSP_RUNTIME_DLL)
        set(_sv_winfsp_dll_default "C:/Program Files (x86)/WinFsp/bin/winfsp-x64.dll")
        if(EXISTS "${_sv_winfsp_dll_default}")
            set(SV_WINFSP_RUNTIME_DLL "${_sv_winfsp_dll_default}")
        endif()
    endif()
    if(EXISTS "${SV_WINFSP_RUNTIME_DLL}")
        add_custom_command(TARGET sv_cli PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SV_WINFSP_RUNTIME_DLL}"
                $<TARGET_FILE_DIR:sv_cli>
            COMMENT "Copy winfsp-x64.dll beside svcli"
        )
    endif()
endif()

target_compile_features(sv_cli PRIVATE cxx_std_20)
sv_apply_warnings(sv_cli)