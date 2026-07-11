add_library(sv_pba_freestanding STATIC
    src/fs_util.c
    src/fs_codec.c
    src/fs_crypto.c
    src/fs_unlock.c
)

target_include_directories(sv_pba_freestanding
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sv_pba_freestanding PUBLIC sv_libsodium)
target_compile_features(sv_pba_freestanding PUBLIC c_std_11)

if(MSVC)
    target_compile_options(sv_pba_freestanding PRIVATE /W4)
else()
    target_compile_options(sv_pba_freestanding PRIVATE -Wall -Wextra)
endif()

message(STATUS "sv_pba_freestanding: C unlock core (host + future EFI)")