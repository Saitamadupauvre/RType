find_package(clap 1.0 CONFIG REQUIRED)

function(game_add_runtime_data target folder source)
    set(destination ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${folder})
    add_custom_target(${target}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${destination}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${source} ${destination}
        VERBATIM
    )
endfunction()

game_add_runtime_data(r-type_scripts scripts ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
game_add_runtime_data(r-type_assets assets ${PROJECT_SOURCE_DIR}/assets)

add_library(r-type_server_cli STATIC server/Cli.cpp)
target_link_libraries(r-type_server_cli PRIVATE clap::clap)
target_include_directories(r-type_server_cli PUBLIC server)
rtype_set_warnings(r-type_server_cli)

add_library(r-type_common STATIC common/ActionCodec.cpp)
target_link_libraries(r-type_common PUBLIC engine::script)
target_include_directories(r-type_common PUBLIC common)
rtype_set_warnings(r-type_common)

add_library(r-type_server_app STATIC server/ServerApp.cpp)
target_link_libraries(r-type_server_app PUBLIC r-type_server_cli r-type_common engine::core engine::network engine::script)
target_include_directories(r-type_server_app PUBLIC server)
rtype_set_warnings(r-type_server_app)

add_executable(r-type_server server/main.cpp)
target_link_libraries(r-type_server PRIVATE r-type_server_app r-type_server_cli engine::core engine::script engine::network)
add_dependencies(r-type_server r-type_scripts)
rtype_set_warnings(r-type_server)

add_library(r-type_client_cli STATIC client/Cli.cpp)
target_link_libraries(r-type_client_cli PRIVATE clap::clap)
target_include_directories(r-type_client_cli PUBLIC client)
rtype_set_warnings(r-type_client_cli)

add_library(r-type_client_controls STATIC client/Controls.cpp)
target_link_libraries(r-type_client_controls PUBLIC engine::graphics engine::script)
target_include_directories(r-type_client_controls PUBLIC client)
rtype_set_warnings(r-type_client_controls)

add_library(r-type_client_draw STATIC client/Starfield.cpp client/WorldDraw.cpp client/SnapshotView.cpp)
target_link_libraries(r-type_client_draw PUBLIC engine::core engine::graphics engine::network)
target_include_directories(r-type_client_draw PUBLIC client)
rtype_set_warnings(r-type_client_draw)

add_library(r-type_client_audio STATIC client/AudioPlayer.cpp)
target_link_libraries(r-type_client_audio PUBLIC engine::core engine::audio engine::network)
target_include_directories(r-type_client_audio PUBLIC client)
rtype_set_warnings(r-type_client_audio)

add_executable(r-type_client client/main.cpp)
target_link_libraries(r-type_client PRIVATE r-type_client_cli r-type_client_controls r-type_client_draw r-type_client_audio r-type_common engine::core engine::network engine::graphics engine::audio)
target_compile_definitions(r-type_client PRIVATE
    GRAPHICS_PLUGIN_FILE="$<TARGET_FILE_NAME:graphics-raylib>"
    AUDIO_PLUGIN_FILE="$<TARGET_FILE_NAME:audio-raylib>"
)
add_dependencies(r-type_client graphics-raylib audio-raylib r-type_assets)
rtype_set_warnings(r-type_client)

option(RTYPE_BUILD_SANDBOX "Build the r-type_sandbox executable" ON)
if(RTYPE_BUILD_SANDBOX)
    add_executable(r-type_sandbox sandbox/main.cpp)
    target_link_libraries(r-type_sandbox PRIVATE r-type_client_draw engine::core engine::graphics)
    target_compile_definitions(r-type_sandbox PRIVATE
        GRAPHICS_PLUGIN_FILE="$<TARGET_FILE_NAME:graphics-raylib>"
    )
    add_dependencies(r-type_sandbox graphics-raylib r-type_assets)
    rtype_set_warnings(r-type_sandbox)
endif()

install(TARGETS r-type_server r-type_client RUNTIME DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION bin/scripts)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION bin/assets PATTERN .gitkeep EXCLUDE)
