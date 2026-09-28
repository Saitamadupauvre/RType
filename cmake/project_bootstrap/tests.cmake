# Managed by project-bootstrap [1331ecd0c26ca733]: edits here are reported by `status`.
option(RTYPE_BUILD_TESTS "Build RType tests" ${PROJECT_IS_TOP_LEVEL})

if(RTYPE_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)

    enable_testing()
    include(GoogleTest)

    add_subdirectory(tests)
endif()
