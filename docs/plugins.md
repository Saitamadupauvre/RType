# Plugins

A plugin is a backend loaded **at runtime** with `dlopen` / `LoadLibrary`,
never linked. The first one is `graphics-raylib`: window, 2D rendering, input
and audio for the client. The server never loads it, so it runs without a
display.

Plugins live in `plugins/<name>/` and are created with the `engine_add_plugin`
CMake helper from `engine/cmake/EngineModule.cmake`.

## The contract

The engine and a plugin agree on two things only:

1. **C++ interfaces defined by the engine** in
   `engine/include/engine/graphics/` (`IRenderer`, `IInput`, `IAudio`,
   `IGraphicsBackend`). They use engine types (`Vec2`, `Rect`, `Color`...),
   never backend types.
2. **Three C functions exported by the plugin**, without C++ name mangling:

```cpp
extern "C" std::uint32_t engine_graphics_api_version();
extern "C" engine::graphics::IGraphicsBackend* engine_graphics_create();
extern "C" void engine_graphics_destroy(engine::graphics::IGraphicsBackend*);
```

| Function | Contract |
|---|---|
| `engine_graphics_api_version` | Returns the `engine::graphics::plugin_api_version` the plugin was built with. |
| `engine_graphics_create` | Allocates the backend. Called once, after the version check. |
| `engine_graphics_destroy` | Frees the backend. Memory is always freed by the plugin that allocated it. |

When loading a plugin, the engine:

1. opens the library, and fails cleanly if it is missing;
2. looks up the three symbols, and fails cleanly if one is missing;
3. compares `engine_graphics_api_version()` with its own
   `plugin_api_version` (`<engine/graphics/PluginApi.hpp>`), and refuses the
   plugin if they differ;
4. calls `engine_graphics_create()` and uses the backend only through the
   interfaces.

Any change to the interfaces increments `plugin_api_version`, so an old plugin
is refused instead of crashing.

## Declaring a plugin

```cmake
engine_add_plugin(graphics-raylib
    SOURCES src/Plugin.cpp
    PRIVATE_DEPENDS raylib
)
```

| Step | Result |
|---|---|
| `add_library(... MODULE ...)` | Loadable library, cannot be linked by mistake. |
| `PREFIX ""` | `graphics-raylib.so`, not `libgraphics-raylib.so`. |
| Output directory | `bin/plugins/`. |
| RPATH `$ORIGIN/..` | The plugin finds `libengine-core.so` in `bin/`. |
| Links `engine::core` | Engine types available to the plugin. |
| Hidden visibility | Only the three C functions are exported. |

## Rules

- The backend library is included only inside its plugin: `raylib.h` appears
  nowhere else. This also avoids the `windows.h` / Raylib name clashes.
- All graphics calls happen on the main thread.
- Timing comes from the engine clock, not from the backend.
- The client draws in virtual units (1920 x 1080); the plugin handles scaling
  and letterboxing.
- Plugin loading is tested: missing library, missing symbol, wrong API
  version.

## Adding a plugin

1. Create `plugins/<name>/` with its `CMakeLists.txt` calling
   `engine_add_plugin`, and add it to `plugins/CMakeLists.txt`.
2. Export the three C functions and implement the engine interfaces.
3. Add the backend library to `vcpkg.json` and link it with
   `PRIVATE_DEPENDS`.
4. Add `add_dependencies(<executable> <name>)` in the executable that loads
   it, so it is built with it.
