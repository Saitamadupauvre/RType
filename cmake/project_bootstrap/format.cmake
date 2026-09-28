# Managed by project-bootstrap [2f9804bae12bfca1]: edits here are reported by `status`.
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    # Same files as the CI format check: C++ sources under src/ include/ tests/ bench/.
    file(GLOB_RECURSE rtype_format_sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.cc"
        "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/include/*.cpp" "${PROJECT_SOURCE_DIR}/include/*.cc"
        "${PROJECT_SOURCE_DIR}/include/*.hpp" "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp" "${PROJECT_SOURCE_DIR}/tests/*.cc"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp" "${PROJECT_SOURCE_DIR}/tests/*.h"
        "${PROJECT_SOURCE_DIR}/bench/*.cpp" "${PROJECT_SOURCE_DIR}/bench/*.cc"
        "${PROJECT_SOURCE_DIR}/bench/*.hpp" "${PROJECT_SOURCE_DIR}/bench/*.h"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${rtype_format_sources}
        COMMENT "Formatting sources"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${rtype_format_sources}
        COMMENT "Checking formatting"
        VERBATIM
    )
endif()
