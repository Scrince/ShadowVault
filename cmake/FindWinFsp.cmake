# Locate WinFsp headers and import libraries for native + FUSE shims.

find_path(WINFSP_INCLUDE_DIR
    NAMES winfsp/winfsp.h fuse3/fuse.h
    PATHS
        "${SV_WINFSP_ROOT}/inc"
        "$ENV{SV_WINFSP_ROOT}/inc"
        "C:/Temp/winfsp-src/inc"
        "C:/Program Files (x86)/WinFsp/inc"
    PATH_SUFFIXES ""
)

find_library(WINFSP_LIBRARY
    NAMES winfsp-x64 winfsp-x86 winfsp-a64 winfsp
    PATHS
        "${SV_WINFSP_ROOT}/opt/fsext/lib"
        "$ENV{SV_WINFSP_ROOT}/opt/fsext/lib"
        "C:/Temp/winfsp-src/opt/fsext/lib"
    PATH_SUFFIXES ""
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WinFsp
    REQUIRED_VARS WINFSP_INCLUDE_DIR WINFSP_LIBRARY
)

if(WinFsp_FOUND AND NOT TARGET WinFsp::winfsp)
    add_library(WinFsp::winfsp UNKNOWN IMPORTED)
    set_target_properties(WinFsp::winfsp PROPERTIES
        IMPORTED_LOCATION "${WINFSP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${WINFSP_INCLUDE_DIR}"
    )

    set(_WINFSP_FUSE_IMPORT_LIB "${CMAKE_SOURCE_DIR}/cmake/winfsp_fuse.lib")
    if(NOT EXISTS "${_WINFSP_FUSE_IMPORT_LIB}")
        set(_WINFSP_FUSE_IMPORT_LIB "${CMAKE_BINARY_DIR}/winfsp_fuse.lib")
    endif()

    if(EXISTS "${_WINFSP_FUSE_IMPORT_LIB}")
        add_library(WinFsp::fuse UNKNOWN IMPORTED)
        set_target_properties(WinFsp::fuse PROPERTIES
            IMPORTED_LOCATION "${_WINFSP_FUSE_IMPORT_LIB}"
        )
    endif()
endif()