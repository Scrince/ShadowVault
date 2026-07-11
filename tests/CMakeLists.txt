include(FetchContent)

FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(Catch2)

set(SV_UNIT_TEST_SOURCES
    unit/test_result.cpp
    unit/test_paths.cpp
    unit/test_cli_args.cpp
    unit/test_crypto_smoke.cpp
    unit/crypto/test_aead.cpp
    unit/crypto/test_kdf.cpp
    unit/crypto/test_hkdf.cpp
    unit/crypto/test_random.cpp
    unit/crypto/test_consttime.cpp
    unit/crypto/test_secure_buffer.cpp
    unit/portable/test_detect.cpp
    unit/portable/test_no_traces.cpp
    unit/container/test_header_codec.cpp
    unit/container/test_superblock.cpp
    unit/container/test_dual_header.cpp
    unit/keyfile/test_combiner.cpp
    unit/keyfile/test_format.cpp
    unit/vault/test_manifest.cpp
    unit/vault/test_manifest_cap.cpp
    unit/vault/test_manifest_hash.cpp
    unit/vault/test_hidden_keys.cpp
    ${CMAKE_SOURCE_DIR}/src/sv_cli/cli_args.cpp
)

set(SV_UNIT_TEST_LIBS
    sv_common
    sv_platform
    sv_portable
    sv_crypto
    sv_container
    sv_keyfile
    sv_recovery
    sv_vault
    Catch2::Catch2WithMain
)

if(SV_BUILD_PBA)
    list(APPEND SV_UNIT_TEST_SOURCES
        unit/pba/test_pba_header.cpp
        unit/pba/test_pba_unlock_fs.cpp
    )
    list(APPEND SV_UNIT_TEST_LIBS sv_pba_core sv_pba_freestanding)
endif()

add_executable(sv_unit_tests ${SV_UNIT_TEST_SOURCES})

target_include_directories(sv_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/sv_cli
        ${CMAKE_SOURCE_DIR}/src/sv_cli/include
        ${CMAKE_SOURCE_DIR}/src/sv_pba/include
        ${CMAKE_SOURCE_DIR}/src/sv_pba/freestanding/include
        ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(sv_unit_tests PRIVATE ${SV_UNIT_TEST_LIBS})

target_compile_features(sv_unit_tests PRIVATE cxx_std_20)

sv_apply_warnings(sv_unit_tests)

set(SV_INTEGRATION_TEST_LIBS
    sv_common
    sv_platform
    sv_crypto
    sv_container
    sv_keyfile
    sv_recovery
    sv_vault
    Catch2::Catch2WithMain
)

if(SV_BUILD_MOUNT)
    list(APPEND SV_INTEGRATION_TEST_SOURCES
        integration/test_svfs.cpp
        integration/test_mount.cpp
    )
    list(APPEND SV_INTEGRATION_TEST_LIBS sv_mount)
endif()

if(SV_BUILD_PBA)
    list(APPEND SV_INTEGRATION_TEST_SOURCES
        integration/test_pba_vault.cpp
        integration/test_pba_lab_vectors.cpp
    )
    list(APPEND SV_INTEGRATION_TEST_LIBS sv_pba_core)
endif()

add_executable(sv_integration_tests
    integration/test_vault_lifecycle.cpp
    integration/test_hidden_volume.cpp
    integration/test_hidden_protection.cpp
    integration/test_hidden_boundary.cpp
    integration/test_wrong_password.cpp
    integration/test_header_backup_restore.cpp
    integration/test_repair.cpp
    integration/test_file_ops.cpp
    integration/test_file_ops_unicode.cpp
    integration/test_verify.cpp
    integration/test_corrupt_header.cpp
    integration/test_interrupted_write.cpp
    integration/test_truncated_vault.cpp
    integration/test_keyfile.cpp
    ${SV_INTEGRATION_TEST_SOURCES}
)

target_include_directories(sv_integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/integration
        ${CMAKE_SOURCE_DIR}/src/sv_mount/include
        ${CMAKE_SOURCE_DIR}/src/sv_pba/include
)

target_link_libraries(sv_integration_tests PRIVATE ${SV_INTEGRATION_TEST_LIBS})

if(SV_BUILD_MOUNT)
    target_compile_definitions(sv_integration_tests PRIVATE SV_HAVE_WINFSP=1)
    if(NOT SV_WINFSP_RUNTIME_DLL)
        set(_sv_winfsp_dll_default "C:/Program Files (x86)/WinFsp/bin/winfsp-x64.dll")
        if(EXISTS "${_sv_winfsp_dll_default}")
            set(SV_WINFSP_RUNTIME_DLL "${_sv_winfsp_dll_default}")
        endif()
    endif()
    if(EXISTS "${SV_WINFSP_RUNTIME_DLL}")
        add_custom_command(TARGET sv_integration_tests PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SV_WINFSP_RUNTIME_DLL}"
                $<TARGET_FILE_DIR:sv_integration_tests>
            COMMENT "Copy winfsp-x64.dll beside sv_integration_tests"
        )
    endif()
endif()

target_compile_features(sv_integration_tests PRIVATE cxx_std_20)
sv_apply_warnings(sv_integration_tests)

if(SV_BUILD_PBA)
    add_executable(sv_pba_export_lab integration/pba_export_lab_main.cpp)
    target_include_directories(sv_pba_export_lab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/integration)
    target_link_libraries(sv_pba_export_lab
        PRIVATE
            sv_common
            sv_platform
            sv_crypto
            sv_container
            sv_vault
            sv_libsodium
    )
    target_compile_features(sv_pba_export_lab PRIVATE cxx_std_20)
    sv_apply_warnings(sv_pba_export_lab)
endif()

add_executable(sv_create_debug integration/test_create_debug_main.cpp)
target_link_libraries(sv_create_debug
    PRIVATE
        sv_common
        sv_platform
        sv_crypto
        sv_container
        sv_keyfile
        sv_recovery
        sv_vault
        sv_libsodium
)
target_compile_features(sv_create_debug PRIVATE cxx_std_20)
sv_apply_warnings(sv_create_debug)

include(CTest)
include(Catch)
catch_discover_tests(sv_unit_tests)
if(SV_BUILD_MOUNT AND EXISTS "${SV_WINFSP_RUNTIME_DLL}")
    get_filename_component(_sv_winfsp_bin_dir "${SV_WINFSP_RUNTIME_DLL}" DIRECTORY)
    catch_discover_tests(sv_integration_tests TEST_DL_PATHS "${_sv_winfsp_bin_dir}")
else()
    catch_discover_tests(sv_integration_tests)
endif()

if(SV_BUILD_FUZZ)
    include(CheckCXXCompilerFlag)

    set(SV_FUZZ_SUPPORTED FALSE)
    set(SV_FUZZ_COMPILE_FLAGS "")
    set(SV_FUZZ_LINK_FLAGS "")
    set(SV_FUZZ_LINK_LIBS "")
    set(SV_FUZZ_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        check_cxx_compiler_flag("-fsanitize=fuzzer-no-link" SV_FUZZ_FLAG_OK)
        if(SV_FUZZ_FLAG_OK)
            set(SV_FUZZ_SUPPORTED TRUE)
            set(SV_FUZZ_COMPILE_FLAGS "-fsanitize=fuzzer-no-link")
            set(SV_FUZZ_LINK_FLAGS "-fsanitize=fuzzer")
        endif()
    elseif(MSVC)
        check_cxx_compiler_flag("/fsanitize=fuzzer" SV_FUZZ_FLAG_OK)
        if(SV_FUZZ_FLAG_OK)
            set(SV_FUZZ_SUPPORTED TRUE)
            set(SV_FUZZ_COMPILE_FLAGS "/fsanitize=fuzzer")
            set(SV_FUZZ_LINK_FLAGS "/INFERASANLIBS")

            get_filename_component(_sv_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            get_filename_component(_sv_msvc_host_dir "${_sv_msvc_bin_dir}" DIRECTORY)
            get_filename_component(_sv_msvc_bin_parent "${_sv_msvc_host_dir}" DIRECTORY)
            get_filename_component(_sv_msvc_root "${_sv_msvc_bin_parent}" DIRECTORY)
            list(APPEND SV_FUZZ_INCLUDE_DIRS "${_sv_msvc_root}/include")
            set(_sv_msvc_asan_dll "${_sv_msvc_bin_dir}/clang_rt.asan_dynamic-x86_64.dll")
        endif()
    endif()

    if(NOT SV_FUZZ_SUPPORTED)
        message(STATUS "SV_BUILD_FUZZ=ON but compiler lacks libFuzzer support; skipping fuzz targets")
    else()
        function(sv_add_fuzz_target target_name)
            cmake_parse_arguments(_SV_FUZZ "" "" "SOURCES;LINK_LIBS" ${ARGN})
            add_executable(${target_name} ${_SV_FUZZ_SOURCES})
            target_include_directories(${target_name} PRIVATE ${SV_FUZZ_INCLUDE_DIRS})
            target_compile_features(${target_name} PRIVATE cxx_std_20)
            target_compile_options(${target_name} PRIVATE ${SV_FUZZ_COMPILE_FLAGS})
            target_link_libraries(${target_name} PRIVATE ${_SV_FUZZ_LINK_LIBS} ${SV_FUZZ_LINK_LIBS})
            if(SV_FUZZ_LINK_FLAGS)
                target_link_options(${target_name} PRIVATE ${SV_FUZZ_LINK_FLAGS})
            endif()
            if(MSVC AND EXISTS "${_sv_msvc_asan_dll}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_sv_msvc_asan_dll}"
                        $<TARGET_FILE_DIR:${target_name}>
                    COMMENT "Copy clang_rt.asan_dynamic-x86_64.dll beside ${target_name}"
                )
            endif()
            sv_apply_warnings(${target_name})
        endfunction()

        sv_add_fuzz_target(fuzz_superblock
            SOURCES fuzz/fuzz_superblock.cpp
            LINK_LIBS sv_container
        )
        sv_add_fuzz_target(fuzz_header
            SOURCES fuzz/fuzz_header.cpp
            LINK_LIBS sv_container
        )
        sv_add_fuzz_target(fuzz_keyfile
            SOURCES fuzz/fuzz_keyfile.cpp
            LINK_LIBS sv_keyfile
        )
        sv_add_fuzz_target(fuzz_manifest
            SOURCES fuzz/fuzz_manifest.cpp
            LINK_LIBS sv_vault
        )
    endif()
endif()