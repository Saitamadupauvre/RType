# Managed by project-bootstrap [2f9804bae12bfca1]: edits here are reported by `status`.
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    # Same files as the CI format check: C++ sources under engine/ plugins/ editor/ runtime/ tests/.
    file(GLOB_RECURSE workspace_format_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/engine/*.cpp" "${PROJECT_SOURCE_DIR}/engine/*.hpp"
        "${PROJECT_SOURCE_DIR}/plugins/*.cpp" "${PROJECT_SOURCE_DIR}/plugins/*.hpp"
        "${PROJECT_SOURCE_DIR}/editor/*.cpp" "${PROJECT_SOURCE_DIR}/editor/*.hpp"
        "${PROJECT_SOURCE_DIR}/runtime/*.cpp" "${PROJECT_SOURCE_DIR}/runtime/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp" "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${workspace_format_sources}
        COMMENT "Formatting sources"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${workspace_format_sources}
        COMMENT "Checking formatting"
        VERBATIM
    )
endif()
