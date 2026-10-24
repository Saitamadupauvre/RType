find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_library(audio-raylib-mixer STATIC src/AudioMixer.cpp)
target_include_directories(audio-raylib-mixer PUBLIC src)
target_link_libraries(audio-raylib-mixer PUBLIC engine::audio)
set_target_properties(audio-raylib-mixer PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
engine_set_warnings(audio-raylib-mixer)

engine_add_plugin(audio-raylib
    SOURCES
        src/Plugin.cpp
        src/RaylibAudio.cpp
    PRIVATE_DEPENDS engine::audio audio-raylib-mixer raylib glfw
)
