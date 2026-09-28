include_guard(GLOBAL)
include(GenerateExportHeader)

option(ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors in engine code" OFF)

set(ENGINE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(ENGINE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")

function(engine_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(ENGINE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
        )
        if(ENGINE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(engine_add_module name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEPENDS;PRIVATE_DEPENDS")
    set(target engine-${name})
    string(TOUPPER "${name}" upper)

    add_library(${target} SHARED ${arg_SOURCES})
    add_library(engine::${name} ALIAS ${target})

    generate_export_header(${target}
        BASE_NAME ENGINE_${upper}
        EXPORT_FILE_NAME "${ENGINE_GENERATED_INCLUDE_DIR}/engine/${name}/Export.hpp"
    )

    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/include>
        $<BUILD_INTERFACE:${ENGINE_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${target}
        PUBLIC ${arg_DEPENDS}
        PRIVATE ${arg_PRIVATE_DEPENDS}
    )
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    engine_set_warnings(${target})

    install(TARGETS ${target}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION bin
    )
endfunction()

function(engine_add_plugin name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;PRIVATE_DEPENDS")

    add_library(${name} MODULE ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE engine::core ${arg_PRIVATE_DEPENDS})
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/$<1:bin/plugins>"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    if(APPLE)
        set_target_properties(${name} PROPERTIES INSTALL_RPATH "@loader_path/..")
    else()
        set_target_properties(${name} PROPERTIES INSTALL_RPATH "$ORIGIN/..")
    endif()
    engine_set_warnings(${name})

    install(TARGETS ${name} LIBRARY DESTINATION bin/plugins)
endfunction()
