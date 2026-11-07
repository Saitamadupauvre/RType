find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(clap 1.0 CONFIG REQUIRED)
find_path(CLAY_INCLUDE_DIR clay.h REQUIRED)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wmissing-designated-field-initializers
    EDITOR_HAS_DESIGNATED_FIELD_WARNING)

function(editor_set_warnings target)
    rtype_set_warnings(${target})
    if(EDITOR_HAS_DESIGNATED_FIELD_WARNING)
        target_compile_options(${target} PRIVATE -Wno-missing-designated-field-initializers)
    endif()
endfunction()

add_library(editor-clay STATIC src/clay/ClayImplementation.cpp)
target_include_directories(editor-clay SYSTEM PUBLIC ${CLAY_INCLUDE_DIR})
set_target_properties(editor-clay PROPERTIES CXX_CLANG_TIDY "")

add_library(editor-core STATIC
    src/action/ApplyCommand.cpp
    src/action/ApplyEdit.cpp
    src/action/Editor.cpp
    src/action/InspectorModel.cpp
    src/action/OpenEditor.cpp
    src/clay/ClayContext.cpp
    src/cli/Cli.cpp
    src/play/PlaySession.cpp
    src/play/ProcessLauncher.cpp
    src/render/Geometry.cpp
    src/render/ScissorStack.cpp
    src/text/FitText.cpp
    src/text/TextEdit.cpp
    src/text/Utf8.cpp
    src/ui/ClickCounter.cpp
    src/ui/CommandTable.cpp
    src/ui/FieldText.cpp
    src/ui/Labels.cpp
    src/ui/Layout.cpp
    src/ui/LetterKey.cpp
    src/ui/Text.cpp
    src/ui/Theme.cpp
    src/ui/Ui.cpp
    src/ui/UiInput.cpp
    src/ui/SmoothScroll.cpp
    src/ui/WidgetState.cpp
    src/ui/sections/BottomPanel.cpp
    src/ui/sections/Inspector.cpp
    src/ui/sections/InspectorFields.cpp
    src/ui/sections/MenuBar.cpp
    src/ui/sections/PanelHeader.cpp
    src/ui/sections/Sidebar.cpp
    src/ui/sections/StatusBar.cpp
    src/ui/sections/Toolbar.cpp
    src/ui/sections/Viewport.cpp
    src/viewport/Handles.cpp
    src/viewport/SceneItems.cpp
    src/viewport/Snap.cpp
    src/viewport/ViewCamera.cpp
    src/ui/widgets/Button.cpp
    src/ui/widgets/Checkbox.cpp
    src/ui/widgets/Dropdown.cpp
    src/ui/widgets/FieldKeys.cpp
    src/ui/widgets/ListRow.cpp
    src/ui/widgets/NumberField.cpp
    src/ui/widgets/TextField.cpp
    src/ui/widgets/Toggle.cpp
)
target_include_directories(editor-core PUBLIC src)
target_link_libraries(editor-core PUBLIC editor-clay engine::editor PRIVATE clap::clap)
editor_set_warnings(editor-core)

set(EDITOR_ASSETS_DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/editor-assets)
add_custom_target(editor-assets
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${EDITOR_ASSETS_DESTINATION}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${EDITOR_ASSETS_DESTINATION}
    VERBATIM
)

add_executable(editor
    src/main.cpp
    src/raylib/ClayRenderer.cpp
    src/raylib/EditorWindow.cpp
    src/raylib/FontCache.cpp
    src/raylib/ViewportPainter.cpp
)
target_link_libraries(editor PRIVATE editor-core raylib glfw)
target_compile_definitions(editor PRIVATE GLFW_INCLUDE_NONE)
add_dependencies(editor editor-assets)
editor_set_warnings(editor)
