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
        src/core/EntityManager.cpp
        src/core/SystemScheduler.cpp
        src/core/event/EventBus.cpp
        src/core/log/Log.cpp
        src/core/log/MemorySink.cpp
        src/core/log/StderrSink.cpp
        src/core/time/IClock.cpp
        src/core/time/SteadyClock.cpp
        src/core/time/ManualClock.cpp
        src/core/time/FixedStepLoop.cpp
)

engine_add_module(script
    SOURCES src/script/ScriptRuntime.cpp
    DEPENDS engine::core
    PRIVATE_DEPENDS sol2::sol2 engine_lua
)

engine_add_module(network
    SOURCES src/network/Module.cpp
    DEPENDS engine::core
)
