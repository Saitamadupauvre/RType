function(engine_add_test module)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    set(target engine_${module}_tests)

    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE engine::${module} GTest::gtest_main)
    target_compile_definitions(${target} PRIVATE
        RTYPE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    rtype_set_warnings(${target})
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST TEST_PREFIX "${module}.")
endfunction()

engine_add_test(core SOURCES engine/core/ModuleTest.cpp)
engine_add_test(script SOURCES engine/script/ModuleTest.cpp)
engine_add_test(network SOURCES
    engine/network/ModuleTest.cpp
    engine/network/ProtocolHandlerTest.cpp
)
target_include_directories(engine_network_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/src/network")
