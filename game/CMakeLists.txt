add_executable(r-type_server server/main.cpp)
target_link_libraries(r-type_server PRIVATE engine::core engine::script engine::network)
rtype_set_warnings(r-type_server)

add_executable(r-type_client client/main.cpp)
target_link_libraries(r-type_client PRIVATE engine::core engine::script engine::network)
add_dependencies(r-type_client graphics-raylib)
rtype_set_warnings(r-type_client)

install(TARGETS r-type_server r-type_client RUNTIME DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION bin/scripts)
