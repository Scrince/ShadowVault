# CMake wrapper for fetched libsodium sources (no upstream CMakeLists).
# Adapted from https://github.com/robinlinden/libsodium-cmake (ISC).

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED LIBSODIUM_ROOT)
    message(FATAL_ERROR "LIBSODIUM_ROOT must point to extracted libsodium sources")
endif()

project(sodium LANGUAGES C)

option(SODIUM_DISABLE_TESTS "Disable libsodium tests" ON)
option(SODIUM_MINIMAL "Minimal libsodium build" ON)

set(_SODIUM_SRC "${LIBSODIUM_ROOT}/src/libsodium")

add_library(sodium STATIC
    stubs.c
    ${_SODIUM_SRC}/crypto_aead/xchacha20poly1305/aead_xchacha20poly1305.c
    ${_SODIUM_SRC}/crypto_core/hchacha20/core_hchacha20.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/ref/blake2b-ref.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/ref/generichash_blake2b.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/ref/blake2b-compress-ref.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/ref/blake2b-compress-ssse3.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/ref/blake2b-compress-sse41.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/ref/blake2b-compress-avx2.c
    ${_SODIUM_SRC}/crypto_generichash/blake2b/generichash_blake2.c
    ${_SODIUM_SRC}/crypto_generichash/crypto_generichash.c
    ${_SODIUM_SRC}/crypto_auth/crypto_auth.c
    ${_SODIUM_SRC}/crypto_auth/hmacsha256/auth_hmacsha256.c
    ${_SODIUM_SRC}/crypto_hash/sha256/hash_sha256.c
    ${_SODIUM_SRC}/crypto_hash/sha256/cp/hash_sha256_cp.c
    ${_SODIUM_SRC}/crypto_pwhash/crypto_pwhash.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2-core.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2-encoding.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2-fill-block-ref.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2-fill-block-ssse3.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2-fill-block-avx2.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2-fill-block-avx512f.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/argon2.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/blake2b-long.c
    ${_SODIUM_SRC}/crypto_pwhash/argon2/pwhash_argon2id.c
    ${_SODIUM_SRC}/crypto_kdf/hkdf/kdf_hkdf_sha256.c
    ${_SODIUM_SRC}/crypto_kdf/crypto_kdf.c
    ${_SODIUM_SRC}/crypto_onetimeauth/poly1305/onetimeauth_poly1305.c
    ${_SODIUM_SRC}/crypto_onetimeauth/poly1305/donna/poly1305_donna.c
    ${_SODIUM_SRC}/crypto_onetimeauth/crypto_onetimeauth.c
    ${_SODIUM_SRC}/crypto_stream/chacha20/stream_chacha20.c
    ${_SODIUM_SRC}/crypto_stream/chacha20/ref/chacha20_ref.c
    ${_SODIUM_SRC}/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-ssse3.c
    ${_SODIUM_SRC}/crypto_stream/chacha20/dolbeau/chacha20_dolbeau-avx2.c
    ${_SODIUM_SRC}/crypto_onetimeauth/poly1305/sse2/poly1305_sse2.c
    ${_SODIUM_SRC}/crypto_stream/crypto_stream.c
    ${_SODIUM_SRC}/crypto_verify/verify.c
    ${_SODIUM_SRC}/sodium/core.c
    ${_SODIUM_SRC}/sodium/runtime.c
    ${_SODIUM_SRC}/sodium/utils.c
    ${_SODIUM_SRC}/sodium/version.c
    ${_SODIUM_SRC}/sodium/codecs.c
    ${_SODIUM_SRC}/randombytes/randombytes.c
    ${_SODIUM_SRC}/randombytes/sysrandom/randombytes_sysrandom.c
    ${_SODIUM_SRC}/randombytes/internal/randombytes_internal_random.c
)

set_target_properties(sodium PROPERTIES C_STANDARD 99)

set(_SODIUM_GEN_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${_SODIUM_GEN_INCLUDE}/sodium")

target_compile_definitions(sodium
    PUBLIC
        SODIUM_STATIC
        $<$<BOOL:${SODIUM_MINIMAL}>:SODIUM_LIBRARY_MINIMAL=1>
    PRIVATE
        CONFIGURED
        $<$<BOOL:${SODIUM_MINIMAL}>:MINIMAL>
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

if(WIN32)
    target_compile_definitions(sodium PRIVATE inline=__inline)
endif()

set(VERSION "1.0.20")
set(SODIUM_LIBRARY_VERSION_MAJOR 26)
set(SODIUM_LIBRARY_VERSION_MINOR 2)
if(SODIUM_MINIMAL)
    set(SODIUM_LIBRARY_MINIMAL_DEF "#define SODIUM_LIBRARY_MINIMAL 1")
endif()

configure_file(
    ${_SODIUM_SRC}/include/sodium/version.h.in
    ${_SODIUM_GEN_INCLUDE}/sodium/version.h
    @ONLY
)

file(READ "${_SODIUM_GEN_INCLUDE}/sodium/version.h" _SV_VERSION_H_CONTENT)
string(REPLACE "#include \"export.h\"" "#include <sodium/export.h>" _SV_VERSION_H_CONTENT "${_SV_VERSION_H_CONTENT}")
file(WRITE "${_SODIUM_GEN_INCLUDE}/sodium/version.h" "${_SV_VERSION_H_CONTENT}")

target_include_directories(sodium
    PUBLIC
        ${_SODIUM_GEN_INCLUDE}
        ${_SODIUM_SRC}/include
    PRIVATE
        ${_SODIUM_SRC}/include/sodium
        ${_SODIUM_GEN_INCLUDE}/sodium
)

