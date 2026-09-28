# Managed by project-bootstrap [1331ecd0c26ca733]: edits here are reported by `status`.
option(RTYPE_BUILD_TESTS "Build RType tests" ${PROJECT_IS_TOP_LEVEL})

if(RTYPE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
        URL_HASH SHA256=63b9c77751a5b8f492486005f67533fdc58682b67476fcb91650be5958d5195a
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        FIND_PACKAGE_ARGS NAMES GTest
    )
    # Third-party code: no clang-tidy, and on Windows do not override the
    # parent project's runtime library.
    block()
        set(CMAKE_CXX_CLANG_TIDY "")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endblock()

    enable_testing()
    include(GoogleTest)

    file(GLOB rtype_test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/*.cpp")
    add_executable(rtype_tests ${rtype_test_sources})
    target_link_libraries(rtype_tests PRIVATE rtype::lib GTest::gtest_main)
    rtype_set_warnings(rtype_tests)
    gtest_discover_tests(rtype_tests DISCOVERY_MODE PRE_TEST)
endif()
