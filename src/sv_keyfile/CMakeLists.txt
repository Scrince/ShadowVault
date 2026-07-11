add_library(sv_keyfile STATIC
    src/combiner.cpp
    src/keyfile_format.cpp
)

target_include_directories(sv_keyfile
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sv_keyfile
    PUBLIC
        sv_common
        sv_crypto
        sv_platform
)

target_compile_features(sv_keyfile PUBLIC cxx_std_20)
sv_apply_warnings(sv_keyfile)