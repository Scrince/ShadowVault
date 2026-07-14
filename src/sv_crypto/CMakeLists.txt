add_library(sv_crypto STATIC
    src/sodium_init.cpp
    src/libsodium_provider.cpp
    src/secure_random.cpp
    src/kdf_engine.cpp
    src/hkdf.cpp
    src/secure_memory.cpp
    src/constant_time.cpp
    src/secure_buffer.cpp
)

target_include_directories(sv_crypto
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sv_crypto
    PUBLIC
        sv_common
        sv_platform
        sv_libsodium
)

target_compile_features(sv_crypto PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(sv_crypto PRIVATE /FS)
endif()

sv_apply_warnings(sv_crypto)
