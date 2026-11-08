find_package(clap 1.0 CONFIG REQUIRED)

add_library(engine-runtime-core STATIC src/Cli.cpp src/Options.cpp)
target_link_libraries(engine-runtime-core PUBLIC engine::app PRIVATE engine::script clap::clap)
target_include_directories(engine-runtime-core PUBLIC src)
workspace_set_warnings(engine-runtime-core)

add_executable(engine-runtime src/main.cpp)
target_link_libraries(engine-runtime PRIVATE engine-runtime-core engine::app)
target_compile_definitions(engine-runtime PRIVATE
    GRAPHICS_PLUGIN_FILE="$<TARGET_FILE_NAME:graphics-raylib>"
    AUDIO_PLUGIN_FILE="$<TARGET_FILE_NAME:audio-raylib>"
)
add_dependencies(engine-runtime graphics-raylib audio-raylib)
workspace_set_warnings(engine-runtime)

install(TARGETS engine-runtime RUNTIME DESTINATION bin)
