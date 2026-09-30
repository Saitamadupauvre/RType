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

engine_add_test(core SOURCES
    engine/core/EntityTest.cpp
    engine/core/EntityPoolTest.cpp
    engine/core/ComponentPoolTest.cpp
    engine/core/ComponentTypeIdTest.cpp
    engine/core/RegistryTest.cpp
    engine/core/event/EventBusTest.cpp
    engine/core/log/LogTest.cpp
    engine/core/log/MemorySinkTest.cpp
)
engine_add_test(script SOURCES engine/script/ModuleTest.cpp)
engine_add_test(network SOURCES engine/network/ModuleTest.cpp)
