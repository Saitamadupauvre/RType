# Managed by project-bootstrap [d65561c15f6f75ba]: edits here are reported by `status`.
include_guard(GLOBAL)
include(GNUInstallDirs)

option(WORKSPACE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Apply the project's warning flags to one of its own targets.
function(workspace_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(WORKSPACE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
        )
        if(WORKSPACE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# <workspace/version.hpp>, generated from project(VERSION ...).
set(WORKSPACE_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated/include")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/version.hpp.in"
    "${WORKSPACE_GENERATED_INCLUDE_DIR}/workspace/version.hpp"
    @ONLY
)
