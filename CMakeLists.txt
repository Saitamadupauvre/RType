cmake_minimum_required(VERSION 3.25)

project(RType
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings helper, version header, install dirs.
include(cmake/project_bootstrap/project.cmake)

add_library(rtype_lib
    src/rtype.cpp
)
add_library(rtype::lib ALIAS rtype_lib)
target_include_directories(rtype_lib PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${RTYPE_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
rtype_set_warnings(rtype_lib)

add_executable(rtype src/main.cpp)
target_link_libraries(rtype PRIVATE rtype::lib)
rtype_set_warnings(rtype)

install(TARGETS rtype RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Added by project-bootstrap
include(cmake/project_bootstrap/tests.cmake)
include(cmake/project_bootstrap/format.cmake)
include(cmake/project_bootstrap/docs.cmake)
