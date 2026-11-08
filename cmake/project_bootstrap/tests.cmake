# Managed by project-bootstrap [1331ecd0c26ca733]: edits here are reported by `status`.
option(WORKSPACE_BUILD_TESTS "Build the workspace tests" ${PROJECT_IS_TOP_LEVEL})

if(WORKSPACE_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)

    enable_testing()
    include(GoogleTest)

    add_subdirectory(tests)
endif()
