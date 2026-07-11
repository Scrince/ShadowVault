# Phase 1 — Pre-Boot Authentication (design gate passed; host-testable core first).

if(NOT SV_BUILD_PBA)
    return()
endif()

add_subdirectory(freestanding)

add_library(sv_pba_core STATIC
    core/pba_header.cpp
    core/pba_unlock.cpp
)

target_include_directories(sv_pba_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(sv_pba_core
    PUBLIC
        sv_common
        sv_container
        sv_pba_freestanding
)

target_compile_features(sv_pba_core PUBLIC cxx_std_20)
sv_apply_warnings(sv_pba_core)

add_subdirectory(efi)

message(STATUS "sv_pba: Phase 1 PBA core enabled")