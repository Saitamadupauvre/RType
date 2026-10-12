find_package(raylib CONFIG REQUIRED)

add_library(graphics-raylib-letterbox STATIC src/Letterbox.cpp)
target_include_directories(graphics-raylib-letterbox PUBLIC src)
target_link_libraries(graphics-raylib-letterbox PUBLIC engine::graphics)
set_target_properties(graphics-raylib-letterbox PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
engine_set_warnings(graphics-raylib-letterbox)

engine_add_plugin(graphics-raylib
    SOURCES
        src/Plugin.cpp
        src/RaylibInput.cpp
        src/RaylibRenderer.cpp
    PRIVATE_DEPENDS engine::graphics graphics-raylib-letterbox raylib
)
