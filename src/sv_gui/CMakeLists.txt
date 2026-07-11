# Milestone 8 — Qt GUI (svgui).
# Set CMAKE_PREFIX_PATH to your Qt 6 kit (e.g. C:/Qt/6.9.0/msvc2022_64 or mingw_64).

if(NOT SV_BUILD_GUI)
    return()
endif()

if(DEFINED SV_QT_ROOT_PATH)
    list(APPEND CMAKE_PREFIX_PATH "${SV_QT_ROOT_PATH}")
endif()

find_package(Qt6 QUIET COMPONENTS Widgets)

if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    add_executable(svgui
        main.cpp
        app.cpp
        mainwindow.cpp
        gui_settings.cpp
        vault_service.cpp
        dangerous_op_dialog.cpp
        dialogs/password_dialog.cpp
        dialogs/preferences_dialog.cpp
        wizards/create_volume_wizard.cpp
        wizards/hidden_volume_wizard.cpp
        include/sv/gui/app.hpp
        include/sv/gui/mainwindow.hpp
        include/sv/gui/gui_settings.hpp
        include/sv/gui/vault_service.hpp
        include/sv/gui/dangerous_op_dialog.hpp
        include/sv/gui/password_dialog.hpp
        include/sv/gui/preferences_dialog.hpp
        include/sv/gui/create_volume_wizard.hpp
        include/sv/gui/hidden_volume_wizard.hpp
    )

    set_target_properties(svgui PROPERTIES
        OUTPUT_NAME svgui
        WIN32_EXECUTABLE TRUE
    )

    target_include_directories(svgui
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_BINARY_DIR}/generated
            ${CMAKE_SOURCE_DIR}/src/sv_vault/include
            ${CMAKE_SOURCE_DIR}/src/sv_container/include
            ${CMAKE_SOURCE_DIR}/src/sv_crypto/include
            ${CMAKE_SOURCE_DIR}/src/sv_recovery/include
            ${CMAKE_SOURCE_DIR}/src/sv_keyfile/include
            ${CMAKE_SOURCE_DIR}/src/sv_portable/include
    )

    target_link_libraries(svgui
        PRIVATE
            Qt6::Widgets
            sv_common
            sv_platform
            sv_portable
            sv_vault
            sv_recovery
            sv_container
            sv_keyfile
            sv_crypto
    )

    if(SV_BUILD_MOUNT)
        target_include_directories(svgui PRIVATE ${CMAKE_SOURCE_DIR}/src/sv_mount/include)
        target_link_libraries(svgui PRIVATE sv_mount)
        target_compile_definitions(svgui PRIVATE SV_HAVE_MOUNT=1)
        if(NOT SV_WINFSP_RUNTIME_DLL)
            set(_sv_winfsp_dll_default "C:/Program Files (x86)/WinFsp/bin/winfsp-x64.dll")
            if(EXISTS "${_sv_winfsp_dll_default}")
                set(SV_WINFSP_RUNTIME_DLL "${_sv_winfsp_dll_default}")
            endif()
        endif()
        if(EXISTS "${SV_WINFSP_RUNTIME_DLL}")
            add_custom_command(TARGET svgui PRE_LINK
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SV_WINFSP_RUNTIME_DLL}"
                    $<TARGET_FILE_DIR:svgui>
                COMMENT "Copy winfsp-x64.dll beside svgui"
            )
        endif()
    endif()

    target_compile_features(svgui PRIVATE cxx_std_20)
    sv_apply_warnings(svgui)

    message(STATUS "svgui: Qt6 Widgets found — building GUI")
else()
    add_executable(svgui svgui_stub.cpp)

    set_target_properties(svgui PROPERTIES OUTPUT_NAME svgui)

    target_include_directories(svgui
        PRIVATE
            ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(svgui PRIVATE sv_common)
    target_compile_features(svgui PRIVATE cxx_std_20)
    sv_apply_warnings(svgui)

    message(STATUS "svgui: Qt6 not found — building stub (set SV_QT_ROOT_PATH or CMAKE_PREFIX_PATH)")
endif()