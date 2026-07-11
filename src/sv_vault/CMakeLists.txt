add_library(sv_vault STATIC
    src/credential_material.cpp
    src/key_manager.cpp
    src/inode.cpp
    src/manifest.cpp
    src/chunk_store.cpp
    src/vault_engine.cpp
    src/file_ops.cpp
    src/hidden_volume.cpp
    src/verify.cpp
    src/session_store.cpp
)

target_include_directories(sv_vault
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sv_vault
    PUBLIC
        sv_common
        sv_platform
        sv_crypto
        sv_container
        sv_keyfile
        sv_recovery
)

target_compile_features(sv_vault PUBLIC cxx_std_20)
sv_apply_warnings(sv_vault)