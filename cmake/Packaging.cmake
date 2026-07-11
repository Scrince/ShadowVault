# Shadow Vault — CPack configuration (ZIP fallback + WiX when available).

include(CPack)

set(CPACK_PACKAGE_NAME "ShadowVault")
set(CPACK_PACKAGE_VENDOR "ShadowVault")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Encrypted vault storage with hidden-volume support")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ShadowVault")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "support@shadowvault.local")

set(CPACK_GENERATOR "ZIP")
if(WIN32)
    list(APPEND CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/packaging/windows/shadowvault.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Shadow Vault")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/shadowvault/shadowvault")
endif()

set(CPACK_ARCHIVE_FILE_NAME "ShadowVault-${PROJECT_VERSION}-win64")
set(CPACK_PACKAGE_FILE_NAME "ShadowVault-${PROJECT_VERSION}-win64")