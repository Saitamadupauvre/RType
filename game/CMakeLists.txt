find_package(clap 1.0 CONFIG REQUIRED)

add_library(r-type_server_cli STATIC server/Cli.cpp)
target_link_libraries(r-type_server_cli PRIVATE clap::clap)
target_include_directories(r-type_server_cli PUBLIC server)
rtype_set_warnings(r-type_server_cli)

add_executable(r-type_server server/main.cpp)
target_link_libraries(r-type_server PRIVATE r-type_server_cli engine::core engine::script engine::network)
rtype_set_warnings(r-type_server)

add_library(r-type_client_cli STATIC client/Cli.cpp)
target_link_libraries(r-type_client_cli PRIVATE clap::clap)
target_include_directories(r-type_client_cli PUBLIC client)
rtype_set_warnings(r-type_client_cli)

add_library(r-type_client_controls STATIC client/Controls.cpp)
target_link_libraries(r-type_client_controls PUBLIC engine::graphics engine::script)
target_include_directories(r-type_client_controls PUBLIC client)
rtype_set_warnings(r-type_client_controls)

add_executable(r-type_client client/main.cpp client/TempDraw.cpp)
target_link_libraries(r-type_client PRIVATE r-type_client_cli r-type_client_controls engine::core engine::script engine::network engine::graphics)
add_custom_command(TARGET r-type_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:r-type_client>/scripts
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:r-type_client>/assets
)
target_compile_definitions(r-type_client PRIVATE
    GRAPHICS_PLUGIN_FILE="$<TARGET_FILE_NAME:graphics-raylib>"
)
add_dependencies(r-type_client graphics-raylib)
rtype_set_warnings(r-type_client)

install(TARGETS r-type_server r-type_client RUNTIME DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION bin/scripts)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION bin/assets PATTERN .gitkeep EXCLUDE)
