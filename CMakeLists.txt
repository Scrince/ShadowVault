cmake_minimum_required(VERSION 3.20)

project(ShadowVault VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    # Allow parallel CL.exe invocations to share the same target .pdb (avoids C1041).
    add_compile_options(/FS)
endif()

option(SV_BUILD_CLI "Build CLI executable" ON)
option(SV_BUILD_GUI "Build Qt GUI" OFF)
option(SV_BUILD_MOUNT "Build mount backends" OFF)
option(SV_BUILD_PBA "Build Phase 1 PBA core (host tests)" OFF)
option(SV_BUILD_PBA_EFI "Build ShadowVaultBoot.efi via EDK2 (requires EDK2_ROOT)" OFF)
option(SV_BUILD_TESTS "Build unit tests" ON)
option(SV_BUILD_FUZZ "Build fuzz targets" OFF)
option(SV_PACKAGE_INSTALLER "Enable CPack install/package targets" ON)
option(SV_SINGLE_EXE_PORTABLE "Embed WinFsp + portable ini into one Windows exe" OFF)

if(SV_SINGLE_EXE_PORTABLE)
    if(NOT WIN32)
        message(FATAL_ERROR "SV_SINGLE_EXE_PORTABLE is only supported on Windows")
    endif()
    set(SV_BUILD_MOUNT ON CACHE BOOL "Build mount backends" FORCE)
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
    endif()
endif()
option(SV_USE_VENDORED_LIBSODIUM "Use external/libsodium" ON)
option(SV_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(SV_ENABLE_LTO "Enable link-time optimization in Release" OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/ShadowVaultVersion.cmake)

add_subdirectory(external)
add_subdirectory(src)

if(SV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SV_PACKAGE_INSTALLER)
    include(cmake/InstallRules.cmake)
    include(cmake/Packaging.cmake)
endif()