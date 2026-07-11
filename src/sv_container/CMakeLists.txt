add_library(sv_container STATIC
    src/layout.cpp
    src/superblock.cpp
    src/header_codec.cpp
    src/container_file.cpp
)

target_include_directories(sv_container
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sv_container
    PUBLIC
        sv_common
        sv_platform
        sv_crypto
)

target_compile_features(sv_container PUBLIC cxx_std_20)
sv_apply_warnings(sv_container)