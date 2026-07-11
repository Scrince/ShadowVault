set(SV_PLATFORM_COMMON_SOURCES
    src/paths.cpp
    src/file_io.cpp
    src/file_lock.cpp
    src/terminal_secret.cpp
    src/platform_context.cpp
    src/memory_lock.cpp
)

if(WIN32)
    set(SV_PLATFORM_PLATFORM_SOURCES
        src/win32/paths_win32.cpp
        src/win32/file_lock_win32.cpp
        src/win32/terminal_secret_win32.cpp
        src/win32/embedded_runtime_win32.cpp
    )
else()
    set(SV_PLATFORM_PLATFORM_SOURCES
        src/posix/paths_posix.cpp
        src/posix/file_lock_posix.cpp
        src/posix/terminal_secret_posix.cpp
    )
endif()

add_library(sv_platform STATIC
    ${SV_PLATFORM_COMMON_SOURCES}
    ${SV_PLATFORM_PLATFORM_SOURCES}
)

target_include_directories(sv_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sv_platform PUBLIC sv_common)
target_compile_features(sv_platform PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(sv_platform PRIVATE /FS)
endif()

if(WIN32)
    target_link_libraries(sv_platform PRIVATE shell32 advapi32)
else()
    target_link_libraries(sv_platform PRIVATE pthread)
endif()

if(SV_SINGLE_EXE_PORTABLE)
    target_compile_definitions(sv_platform PRIVATE SV_EMBED_WINFSP=1)
endif()