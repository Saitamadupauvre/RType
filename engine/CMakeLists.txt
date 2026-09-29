include(cmake/EngineModule.cmake)

engine_add_module(core
    SOURCES src/core/EntityPool.cpp
)

engine_add_module(script
    SOURCES src/script/Module.cpp
    DEPENDS engine::core
)

engine_add_module(network
    SOURCES src/network/Module.cpp
    DEPENDS engine::core
)
