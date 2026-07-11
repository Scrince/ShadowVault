# Milestone 9 — mount layer (SVFS + FUSE/WinFsp backends).

add_library(sv_mount STATIC
    src/mount_manager.cpp
    src/virtual_block_device.cpp
    src/svfs.cpp
    src/chunk_cache.cpp
    src/backends/stub_backend.cpp
    src/backends/fuse_backend.cpp
)

target_include_directories(sv_mount
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/sv_vault/include
)

target_link_libraries(sv_mount
    PUBLIC
        sv_common
        sv_platform
        sv_vault
)

target_compile_features(sv_mount PUBLIC cxx_std_20)
sv_apply_warnings(sv_mount)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(WIN32)
    if(NOT SV_WINFSP_ROOT)
        set(SV_WINFSP_ROOT "C:/Temp/winfsp-src" CACHE PATH "WinFsp source tree with inc/ and opt/fsext/lib/")
    endif()
    find_package(WinFsp QUIET)
    if(WinFsp_FOUND)
        target_compile_definitions(sv_mount PRIVATE SV_HAVE_WINFSP=1)
        target_link_libraries(sv_mount PUBLIC WinFsp::winfsp)
        if(TARGET WinFsp::fuse)
            target_link_libraries(sv_mount PUBLIC WinFsp::fuse)
        endif()
        set(SV_WINFSP_DLL_DIR "C:/Program Files (x86)/WinFsp/bin" CACHE PATH "WinFsp runtime DLL directory")
        set(SV_WINFSP_RUNTIME_DLL "${SV_WINFSP_DLL_DIR}/winfsp-x64.dll" CACHE FILEPATH "WinFsp runtime DLL")
        set(SV_WINFSP_RUNTIME_DLL "${SV_WINFSP_RUNTIME_DLL}" PARENT_SCOPE)
        set(SV_WINFSP_DLL_DIR "${SV_WINFSP_DLL_DIR}" PARENT_SCOPE)
        message(STATUS "sv_mount: WinFsp FUSE backend enabled")
    else()
        message(STATUS "sv_mount: WinFsp not found — using stub backend (set SV_WINFSP_ROOT)")
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FUSE3 QUIET fuse3)
        if(FUSE3_FOUND)
            target_compile_definitions(sv_mount PRIVATE SV_HAVE_FUSE3=1)
            target_link_libraries(sv_mount PRIVATE ${FUSE3_LIBRARIES})
            target_include_directories(sv_mount PRIVATE ${FUSE3_INCLUDE_DIRS})
            message(STATUS "sv_mount: libfuse3 backend enabled")
        endif()
    endif()
endif()