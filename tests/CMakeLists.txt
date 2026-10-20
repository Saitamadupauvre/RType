function(engine_add_test module)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    set(target engine_${module}_tests)

    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE engine::${module} GTest::gtest_main)
    target_compile_definitions(${target} PRIVATE
        RTYPE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    rtype_set_warnings(${target})
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "")
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST TEST_PREFIX "${module}.")
endfunction()

engine_add_test(core SOURCES
    engine/core/EntityTest.cpp
    engine/core/EntityPoolTest.cpp
    engine/core/ComponentPoolTest.cpp
    engine/core/ComponentTypeIdTest.cpp
    engine/core/EntityManagerTest.cpp
    engine/core/SystemSchedulerTest.cpp
    engine/core/event/EventBusTest.cpp
    engine/core/log/LogTest.cpp
    engine/core/log/MemorySinkTest.cpp
    engine/core/NonCopyableNonMovableTest.cpp
    engine/core/time/ManualClockTest.cpp
    engine/core/time/FixedStepLoopTest.cpp
    engine/core/Vec2Test.cpp
    engine/core/ComponentPhysicsTest.cpp
    engine/core/ComponentsTest.cpp
    engine/core/DynamicLibraryTest.cpp
)
engine_add_test(script SOURCES
    engine/script/ScriptRuntimeTest.cpp
    engine/script/ScriptDeclarationsTest.cpp
    engine/script/ScriptRequireTest.cpp
    engine/script/EntityHandleTest.cpp
    engine/script/TickTest.cpp
    engine/script/PlayersTest.cpp
)
engine_add_test(network SOURCES
    engine/network/ClientTest.cpp
    engine/network/ModuleTest.cpp
    engine/network/ServerTest.cpp
    engine/network/bytes/ByteReaderTest.cpp
    engine/network/bytes/ByteWriterTest.cpp
    engine/network/decoder/PayloadDecoderTest.cpp
    engine/network/encoder/PayloadEncoderTest.cpp
    engine/network/event/GameEventsTest.cpp
    engine/network/protocol/ProtocolHandlerTest.cpp
    engine/network/protocol/SequenceTest.cpp
    engine/network/protocol/TypeTableTest.cpp
    engine/network/server/ServerNetworkTest.cpp
    engine/network/session/ClientConnectionTest.cpp
    engine/network/session/ServerSessionsTest.cpp
    engine/network/snapshot/SnapshotBuilderTest.cpp
    engine/network/snapshot/SnapshotReceiverTest.cpp
    engine/network/socket/SocketErrorTest.cpp
    engine/network/socket/UdpSocketTest.cpp
    engine/network/stats/TrafficMeterTest.cpp
    engine/network/thread/DatagramHandlerTest.cpp
    engine/network/thread/MessageQueueTest.cpp
    engine/network/thread/NetworkIoTest.cpp
)
target_include_directories(engine_network_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/engine/src/network"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/network"
)
engine_add_test(physics SOURCES
    engine/physics/PhysicsSystemTest.cpp
    engine/physics/CollisionsTest.cpp
)
engine_add_test(graphics SOURCES
    engine/graphics/BackendTest.cpp
    engine/graphics/TypesTest.cpp
    engine/graphics/PluginLoaderTest.cpp
)

function(engine_add_test_library name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LINKS")
    add_library(${name} MODULE ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE ${arg_LINKS})
    set_target_properties(${name} PROPERTIES PREFIX "")
    rtype_set_warnings(${name})
endfunction()

function(engine_test_use_library test_target macro library)
    target_compile_definitions(${test_target} PRIVATE ${macro}="$<TARGET_FILE:${library}>")
    add_dependencies(${test_target} ${library})
endfunction()

engine_add_test_library(test-sample-library SOURCES engine/core/fixtures/SampleLibrary.cpp)
engine_test_use_library(engine_core_tests SAMPLE_LIBRARY_PATH test-sample-library)

foreach(plugin IN ITEMS Valid WrongVersion MissingCreate NullBackend)
    string(TOLOWER "${plugin}" lower)
    engine_add_test_library(test-plugin-${lower}
        SOURCES engine/graphics/fixtures/${plugin}Plugin.cpp
        LINKS engine::graphics
    )
endforeach()
engine_test_use_library(engine_graphics_tests VALID_PLUGIN_PATH test-plugin-valid)
engine_test_use_library(engine_graphics_tests WRONG_VERSION_PLUGIN_PATH test-plugin-wrongversion)
engine_test_use_library(engine_graphics_tests MISSING_CREATE_PLUGIN_PATH test-plugin-missingcreate)
engine_test_use_library(engine_graphics_tests NULL_BACKEND_PLUGIN_PATH test-plugin-nullbackend)
engine_test_use_library(engine_graphics_tests RAYLIB_PLUGIN_PATH graphics-raylib)

function(game_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LINKS")
    set(target game_${name}_tests)

    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE ${arg_LINKS} GTest::gtest_main)
    target_compile_definitions(${target} PRIVATE
        RTYPE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    rtype_set_warnings(${target})
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "")
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST TEST_PREFIX "game.${name}.")
endfunction()

function(plugin_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LINKS")
    set(target plugin_${name}_tests)

    add_executable(${target} ${arg_SOURCES})
    target_link_libraries(${target} PRIVATE ${arg_LINKS} GTest::gtest_main)
    rtype_set_warnings(${target})
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST TEST_PREFIX "plugin.${name}.")
endfunction()

plugin_add_test(graphics-raylib
    SOURCES
        plugins/graphics-raylib/LetterboxTest.cpp
        plugins/graphics-raylib/KeyMapTest.cpp
        plugins/graphics-raylib/TextureTableTest.cpp
    LINKS graphics-raylib-letterbox graphics-raylib-keymap graphics-raylib-textures
)

game_add_test(server SOURCES game/server/CliTest.cpp game/server/ServerAppTest.cpp
    LINKS r-type_server_cli r-type_server_app)
game_add_test(client SOURCES game/client/CliTest.cpp game/client/ControlsTest.cpp
    game/client/StarfieldTest.cpp game/client/WorldDrawTest.cpp
    game/client/TemporaryNetworkViewTest.cpp
    LINKS r-type_client_cli r-type_client_controls r-type_client_draw
    r-type_client_temporary_network)
game_add_test(common SOURCES game/common/ActionCodecTest.cpp LINKS r-type_common)
