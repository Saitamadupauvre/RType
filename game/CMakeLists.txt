find_package(clap 1.0 CONFIG REQUIRED)

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
add_custom_command(TARGET r-type_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:r-type_server>/scripts
)
rtype_set_warnings(r-type_server)

add_library(r-type_client_cli STATIC client/Cli.cpp)
target_link_libraries(r-type_client_cli PRIVATE clap::clap)
target_include_directories(r-type_client_cli PUBLIC client)
rtype_set_warnings(r-type_client_cli)

add_library(r-type_client_controls STATIC client/Controls.cpp)
target_link_libraries(r-type_client_controls PUBLIC engine::graphics engine::script)
target_include_directories(r-type_client_controls PUBLIC client)
rtype_set_warnings(r-type_client_controls)

add_library(r-type_client_draw STATIC client/Starfield.cpp client/WorldDraw.cpp)
target_link_libraries(r-type_client_draw PUBLIC engine::core engine::graphics)
target_include_directories(r-type_client_draw PUBLIC client)
rtype_set_warnings(r-type_client_draw)

add_library(r-type_client_temporary_network STATIC client/TemporaryNetworkView.cpp)
target_link_libraries(r-type_client_temporary_network PUBLIC engine::graphics engine::network)
target_include_directories(r-type_client_temporary_network PUBLIC client)
rtype_set_warnings(r-type_client_temporary_network)

add_executable(r-type_client client/main.cpp)
target_link_libraries(r-type_client PRIVATE r-type_client_cli r-type_client_controls r-type_client_draw r-type_client_temporary_network r-type_common engine::core engine::script engine::network engine::graphics)
add_custom_command(TARGET r-type_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:r-type_client>/assets
)
target_compile_definitions(r-type_client PRIVATE
    GRAPHICS_PLUGIN_FILE="$<TARGET_FILE_NAME:graphics-raylib>"
)
add_dependencies(r-type_client graphics-raylib)
rtype_set_warnings(r-type_client)

option(RTYPE_BUILD_SANDBOX "Build the r-type_sandbox executable" ON)
if(RTYPE_BUILD_SANDBOX)
    add_executable(r-type_sandbox sandbox/main.cpp)
    target_link_libraries(r-type_sandbox PRIVATE r-type_client_draw engine::core engine::graphics)
    add_custom_command(TARGET r-type_sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:r-type_sandbox>/assets
    )
    target_compile_definitions(r-type_sandbox PRIVATE
        GRAPHICS_PLUGIN_FILE="$<TARGET_FILE_NAME:graphics-raylib>"
    )
    add_dependencies(r-type_sandbox graphics-raylib)
    rtype_set_warnings(r-type_sandbox)
endif()

install(TARGETS r-type_server r-type_client RUNTIME DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION bin/scripts)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION bin/assets PATTERN .gitkeep EXCLUDE)
