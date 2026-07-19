

include(GNUInstallDirs)

if(SV_BUILD_CLI)
    install(TARGETS sv_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(SV_BUILD_GUI AND TARGET svgui)
    get_target_property(_svgui_output svgui OUTPUT_NAME)
    if(NOT _svgui_output)
        set(_svgui_output svgui)
    endif()
    if(TARGET Qt6::Widgets)
        install(TARGETS svgui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

if(SV_BUILD_MOUNT AND SV_WINFSP_RUNTIME_DLL AND EXISTS "${SV_WINFSP_RUNTIME_DLL}")
    install(FILES "${SV_WINFSP_RUNTIME_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_SOURCE_DIR}/packaging/THIRD_PARTY_NOTICES.txt"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(FILES "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(FILES "${CMAKE_SOURCE_DIR}/docs/cli-spec.md"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(FILES "${CMAKE_SOURCE_DIR}/packaging/windows/shadowvault.ini.example"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    RENAME shadowvault.ini.example
)

if(EXISTS "${CMAKE_SOURCE_DIR}/packaging/completion/sv.bash")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/completion/sv.bash"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME svcli
    )
endif()

if(SV_BUILD_PBA AND EXISTS "${SV_PBA_EFI_OUTPUT}")
    install(FILES "${SV_PBA_EFI_OUTPUT}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/shadowvault/pba
        RENAME ShadowVaultBoot.efi
    )
endif()