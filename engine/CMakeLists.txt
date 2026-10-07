include(cmake/EngineModule.cmake)

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
    SOURCES src/script/Module.cpp
    DEPENDS engine::core
)

engine_add_module(network
    SOURCES
        src/network/Module.cpp
        src/network/ProtocolHandler.cpp
    DEPENDS engine::core
)
