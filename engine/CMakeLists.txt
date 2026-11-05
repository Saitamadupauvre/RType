include(cmake/EngineModule.cmake)

find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(engine_lua INTERFACE)
target_include_directories(engine_lua INTERFACE ${LUA_INCLUDE_DIR})
target_link_libraries(engine_lua INTERFACE ${LUA_LIBRARIES})

engine_add_module(core
    SOURCES
        src/core/AssetCache.cpp
        src/core/EntityPool.cpp
        src/core/Camera.cpp
        src/core/ComponentTypeId.cpp
        src/core/DynamicLibrary.cpp
        src/core/EntityManager.cpp
        src/core/FieldRegistry.cpp
        src/core/SystemScheduler.cpp
        src/core/Transform.cpp
        src/core/console/Console.cpp
        src/core/console/CVars.cpp
        src/core/console/LogCapture.cpp
        src/core/event/EventBus.cpp
        src/core/log/Log.cpp
        src/core/log/MemorySink.cpp
        src/core/log/StderrSink.cpp
        src/core/metrics/Metrics.cpp
        src/core/metrics/ProcessUsage.cpp
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

engine_add_module(navigation
    SOURCES
        src/navigation/NavGrid.cpp
        src/navigation/Pathfinding.cpp
        src/navigation/Smoothing.cpp
)

engine_add_module(script
    SOURCES
        src/script/ApiSet.cpp
        src/script/ClientApi.cpp
        src/script/EditorApi.cpp
        src/script/ScriptRuntime.cpp
        src/script/ScriptRuntimeBuilder.cpp
        src/script/ServerApi.cpp
        src/script/assets/DeclaredAssets.cpp
        src/script/audio/Audio.cpp
        src/script/client/ClientAudio.cpp
        src/script/client/ClientCamera.cpp
        src/script/client/ClientEvents.cpp
        src/script/client/HudCanvas.cpp
        src/script/client/LocalSpawn.cpp
        src/script/entity/Entities.cpp
        src/script/entity/EntityFields.cpp
        src/script/entity/ScriptBody.cpp
        src/script/entity/Instances.cpp
        src/script/entity/ScriptFields.cpp
        src/script/event/Events.cpp
        src/script/input/LuaInput.cpp
        src/script/loading/Declarations.cpp
        src/script/loading/ModuleLoader.cpp
        src/script/loading/NameList.cpp
        src/script/loading/ScriptFiles.cpp
        src/script/loading/Sections.cpp
        src/script/lua/LuaArgs.cpp
        src/script/lua/LuaErrors.cpp
        src/script/lua/LuaFieldValue.cpp
        src/script/navigation/Navigation.cpp
        src/script/player/Players.cpp
        src/script/scene/GameFolder.cpp
        src/script/scene/JsonFieldValue.cpp
        src/script/scene/JsonFile.cpp
        src/script/scene/SceneFile.cpp
        src/script/scene/SceneSwitch.cpp
    DEPENDS engine::core
    PRIVATE_DEPENDS engine::physics engine::navigation sol2::sol2 engine_lua
        nlohmann_json::nlohmann_json
)
target_include_directories(engine-script PRIVATE src/script)

engine_add_module(network
    SOURCES
        src/network/Client.cpp
        src/network/Module.cpp
        src/network/Server.cpp
        src/network/Fields.cpp
        src/network/StringTable.cpp
        src/network/Transport.cpp
        src/network/decoder/EventReader.cpp
        src/network/decoder/PayloadDecoder.cpp
        src/network/decoder/ValueReader.cpp
        src/network/encoder/PayloadEncoder.cpp
        src/network/event/GameEvents.cpp
        src/network/input/InputCodec.cpp
        src/network/protocol/FieldWire.cpp
        src/network/protocol/ProtocolHandler.cpp
        src/network/protocol/ReceivedStrings.cpp
        src/network/protocol/Validation.cpp
        src/network/protocol/WireValue.cpp
        src/network/server/ServerNetwork.cpp
        src/network/session/ClientConnection.cpp
        src/network/session/ServerSessions.cpp
        src/network/snapshot/ReplicatedFields.cpp
        src/network/snapshot/SnapshotBuilder.cpp
        src/network/snapshot/SnapshotReceiver.cpp
        src/network/socket/SocketError.cpp
        src/network/stats/TrafficMeter.cpp
        src/network/socket/UdpSocket.cpp
        src/network/thread/DatagramHandler.cpp
        src/network/thread/NetworkIo.cpp
        src/network/transport/LoopbackLinks.cpp
        src/network/transport/UdpLinks.cpp
    DEPENDS engine::core
)
target_include_directories(engine-network PRIVATE src/network)

engine_add_module(graphics
    SOURCES
        src/graphics/InputCapture.cpp
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

engine_add_module(app
    SOURCES
        src/app/assets/AssetPreloader.cpp
        src/app/assets/AudioPlayer.cpp
        src/app/simulation/CameraFollow.cpp
        src/app/render/CameraRig.cpp
        src/app/ClientApp.cpp
        src/app/Fields.cpp
        src/app/render/HudDraw.cpp
        src/app/Launch.cpp
        src/app/render/LoadingScreen.cpp
        src/app/render/MetricsOverlay.cpp
        src/app/simulation/PhysicsStep.cpp
        src/app/ServerApp.cpp
        src/app/assets/ShaderLibrary.cpp
        src/app/render/SnapshotView.cpp
        src/app/render/SpriteLayout.cpp
        src/app/assets/TextureLibrary.cpp
        src/app/render/WorldDraw.cpp
    DEPENDS engine::core engine::script engine::network engine::graphics engine::audio
    PRIVATE_DEPENDS engine::physics
)
target_include_directories(engine-app PRIVATE src/app)

engine_add_module(editor
    SOURCES
        src/editor/EditorSession.cpp
        src/editor/History.cpp
        src/editor/SceneCommands.cpp
        src/editor/SceneDocument.cpp
    DEPENDS engine::core engine::script
)
