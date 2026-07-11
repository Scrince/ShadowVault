add_library(sv_recovery STATIC
    src/journal.cpp
    src/replay.cpp
    src/repair.cpp
)

target_include_directories(sv_recovery
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sv_recovery
    PUBLIC
        sv_common
        sv_platform
        sv_crypto
        sv_container
)

target_compile_features(sv_recovery PUBLIC cxx_std_20)
sv_apply_warnings(sv_recovery)