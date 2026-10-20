find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_library(graphics-raylib-letterbox STATIC src/Letterbox.cpp)
target_include_directories(graphics-raylib-letterbox PUBLIC src)
target_link_libraries(graphics-raylib-letterbox PUBLIC engine::graphics)
set_target_properties(graphics-raylib-letterbox PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
engine_set_warnings(graphics-raylib-letterbox)

add_library(graphics-raylib-keymap STATIC src/KeyMap.cpp)
target_include_directories(graphics-raylib-keymap
    PUBLIC src
    PRIVATE $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(graphics-raylib-keymap PUBLIC engine::graphics)
set_target_properties(graphics-raylib-keymap PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
engine_set_warnings(graphics-raylib-keymap)

add_library(graphics-raylib-textures INTERFACE)
target_include_directories(graphics-raylib-textures INTERFACE src)
target_link_libraries(graphics-raylib-textures INTERFACE engine::graphics)

engine_add_plugin(graphics-raylib
    SOURCES
        src/Plugin.cpp
        src/RaylibInput.cpp
        src/RaylibRenderer.cpp
        src/TempDraw.cpp
    PRIVATE_DEPENDS engine::graphics graphics-raylib-letterbox graphics-raylib-keymap raylib glfw
)
