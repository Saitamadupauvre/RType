include(cmake/EngineModule.cmake)

find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

add_library(engine_lua INTERFACE)
target_include_directories(engine_lua INTERFACE ${LUA_INCLUDE_DIR})
target_link_libraries(engine_lua INTERFACE ${LUA_LIBRARIES})

engine_add_module(core
    SOURCES
        src/core/EntityPool.cpp
        src/core/ComponentTypeId.cpp
        src/core/DynamicLibrary.cpp
        src/core/EntityManager.cpp
        src/core/FieldRegistry.cpp
        src/core/SystemScheduler.cpp
        src/core/event/EventBus.cpp
        src/core/log/Log.cpp
        src/core/log/MemorySink.cpp
        src/core/log/StderrSink.cpp
        src/core/time/IClock.cpp
        src/core/time/SteadyClock.cpp
        src/core/time/ManualClock.cpp
        src/core/time/FixedStepLoop.cpp
    PRIVATE_DEPENDS ${CMAKE_DL_LIBS}
)

engine_add_module(physics
    SOURCES
        src/physics/PhysicsSystem.cpp
        src/physics/Collisions.cpp
        src/physics/Fields.cpp
    DEPENDS engine::core
)

engine_add_module(script
    SOURCES
        src/script/Audio.cpp
        src/script/Declarations.cpp
        src/script/Entities.cpp
        src/script/EntityFields.cpp
        src/script/LuaFieldValue.cpp
        src/script/ModuleLoader.cpp
        src/script/Players.cpp
        src/script/ScriptFiles.cpp
        src/script/ScriptRuntime.cpp
    DEPENDS engine::core
    PRIVATE_DEPENDS engine::physics sol2::sol2 engine_lua
)

engine_add_module(network
    SOURCES
        src/network/Client.cpp
        src/network/Module.cpp
        src/network/Server.cpp
        src/network/StringTable.cpp
        src/network/decoder/EventReader.cpp
        src/network/decoder/PayloadDecoder.cpp
        src/network/encoder/PayloadEncoder.cpp
        src/network/event/GameEvents.cpp
        src/network/protocol/ProtocolHandler.cpp
        src/network/protocol/TypeTable.cpp
        src/network/protocol/Validation.cpp
        src/network/server/ServerNetwork.cpp
        src/network/session/ClientConnection.cpp
        src/network/session/ServerSessions.cpp
        src/network/snapshot/SnapshotBuilder.cpp
        src/network/snapshot/SnapshotReceiver.cpp
        src/network/socket/SocketError.cpp
        src/network/stats/TrafficMeter.cpp
        src/network/socket/UdpSocket.cpp
        src/network/thread/DatagramHandler.cpp
        src/network/thread/NetworkIo.cpp
    DEPENDS engine::core
)
target_include_directories(engine-network PRIVATE src/network)

engine_add_module(graphics
    SOURCES
        src/graphics/Interfaces.cpp
        src/graphics/PluginLoader.cpp
    DEPENDS engine::core
)

engine_add_module(audio
    SOURCES
        src/audio/Interfaces.cpp
        src/audio/NullAudio.cpp
        src/audio/PluginLoader.cpp
    DEPENDS engine::core
)
