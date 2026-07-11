add_library(sv_portable STATIC
    src/portable_config.cpp
    src/traveler_paths.cpp
)

target_include_directories(sv_portable
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sv_portable PUBLIC sv_common sv_platform)
target_compile_features(sv_portable PUBLIC cxx_std_20)