add_library(sv_common STATIC
    src/logger.cpp
    src/result.cpp
    src/crc32.cpp
)

target_include_directories(sv_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(sv_common PUBLIC cxx_std_20)

sv_apply_warnings(sv_common)