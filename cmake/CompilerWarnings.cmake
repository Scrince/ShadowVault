function(sv_apply_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target_name} PRIVATE
            /W4
            /permissive-
            /utf-8
        )
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror=return-type
        )
    endif()
endfunction()