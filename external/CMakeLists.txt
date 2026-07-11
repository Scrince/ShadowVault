# Third-party dependencies for Shadow Vault.
# Vendored copies in external/libsodium and external/argon2 take precedence.

include(FetchContent)

# ---------------------------------------------------------------------------
# libsodium 1.0.20
# ---------------------------------------------------------------------------
set(_SV_LIBSODIUM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libsodium")

if(EXISTS "${_SV_LIBSODIUM_SOURCE_DIR}/src/libsodium/include/sodium.h")
    message(STATUS "ShadowVault: using vendored libsodium at ${_SV_LIBSODIUM_SOURCE_DIR}")
    set(LIBSODIUM_ROOT "${_SV_LIBSODIUM_SOURCE_DIR}")
else()
    message(STATUS "ShadowVault: fetching libsodium 1.0.20-RELEASE via FetchContent")
    FetchContent_Declare(
        libsodium_src
        GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
        GIT_TAG 1.0.20-RELEASE
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(libsodium_src)
    if(NOT libsodium_src_POPULATED)
        FetchContent_Populate(libsodium_src)
    endif()
    set(LIBSODIUM_ROOT "${libsodium_src_SOURCE_DIR}")
endif()

set(LIBSODIUM_ROOT "${LIBSODIUM_ROOT}" CACHE INTERNAL "libsodium source root")
add_subdirectory(libsodium_cmake ${CMAKE_BINARY_DIR}/external/libsodium_cmake EXCLUDE_FROM_ALL)
add_library(sv_libsodium ALIAS sodium)

# ---------------------------------------------------------------------------
# argon2 (PHC reference implementation)
# ---------------------------------------------------------------------------
set(_SV_ARGON2_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/argon2")
set(SV_HAS_ARGON2 OFF)

if(EXISTS "${_SV_ARGON2_SOURCE_DIR}/include/argon2.h")
    message(STATUS "ShadowVault: using vendored argon2 at ${_SV_ARGON2_SOURCE_DIR}")
    set(_SV_ARGON2_ROOT "${_SV_ARGON2_SOURCE_DIR}")
else()
    message(STATUS "ShadowVault: fetching argon2 PHC reference (20190702) via FetchContent")
    FetchContent_Declare(
        argon2_src
        GIT_REPOSITORY https://github.com/P-H-C/phc-winner-argon2.git
        GIT_TAG 20190702
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(argon2_src)
    if(NOT argon2_src_POPULATED)
        FetchContent_Populate(argon2_src)
    endif()
    set(_SV_ARGON2_ROOT "${argon2_src_SOURCE_DIR}")
endif()

if(WIN32)
    message(STATUS "ShadowVault: argon2 PHC reference skipped on Windows; using libsodium crypto_pwhash_argon2id")
elseif(EXISTS "${_SV_ARGON2_ROOT}/include/argon2.h")
    set(SV_ARGON2_SOURCES
        "${_SV_ARGON2_ROOT}/src/argon2.c"
        "${_SV_ARGON2_ROOT}/src/core.c"
        "${_SV_ARGON2_ROOT}/src/encoding.c"
        "${_SV_ARGON2_ROOT}/src/thread.c"
        "${_SV_ARGON2_ROOT}/src/blake2/blake2b.c"
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
        list(APPEND SV_ARGON2_SOURCES "${_SV_ARGON2_ROOT}/src/opt.c")
    else()
        list(APPEND SV_ARGON2_SOURCES "${_SV_ARGON2_ROOT}/src/ref.c")
    endif()

    add_library(sv_argon2 STATIC ${SV_ARGON2_SOURCES})
    target_include_directories(sv_argon2 PUBLIC "${_SV_ARGON2_ROOT}/include")
    target_compile_definitions(sv_argon2 PRIVATE A2_VISCTL=0)
    set_target_properties(sv_argon2 PROPERTIES C_STANDARD 99)
    if(MSVC)
        # PHC argon2 uses C99 inline/restrict; MSVC needs explicit mappings.
        target_compile_definitions(sv_argon2 PRIVATE
            _CRT_SECURE_NO_WARNINGS
            __inline=inline
            inline=__inline
            restrict=__restrict
        )
        target_compile_options(sv_argon2 PRIVATE /wd4244 /wd4267)
    endif()
    if(NOT WIN32)
        target_link_libraries(sv_argon2 PUBLIC pthread)
    endif()
    set(SV_HAS_ARGON2 ON CACHE INTERNAL "")
else()
    message(WARNING "ShadowVault: argon2 sources unavailable; KDF will return NotImplemented")
endif()