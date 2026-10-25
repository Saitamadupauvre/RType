# Plugins

A plugin is a backend loaded **at runtime** with `dlopen` / `LoadLibrary`,
never linked. There are two kinds, each with its own interface and loader:

| Kind | Interface | Loader | Plugin today |
|---|---|---|---|
| Graphics | `engine/graphics/IGraphicsBackend.hpp` | `load_graphics_plugin()` | `graphics-raylib`: window, 2D rendering, input |
| Audio | `engine/audio/IAudio.hpp` | `load_audio_plugin()` | `audio-raylib`: sound effects, streamed music |

Audio is not part of the graphics backend, so the sound library can be
replaced (miniaudio, OpenAL...) by writing a new audio plugin, without
touching graphics. Only the client loads plugins: the server runs without a
display or a sound card.

Plugins live in `plugins/<name>/` and are created with the `engine_add_plugin`
CMake helper from `engine/cmake/EngineModule.cmake`.

## The contract

The graphics contract is described below; the audio contract is the same with
`audio` in place of `graphics` (see [audio-raylib](#audio-raylib)).

The engine and a plugin agree on two things only:

1. **C++ interfaces defined by the engine** in
   `engine/include/engine/graphics/` (`IRenderer`, `IInput`,
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

`engine::graphics::load_graphics_plugin(path)`
(`<engine/graphics/PluginLoader.hpp>`) loads a plugin. It:

1. opens the library, and fails cleanly if it is missing;
2. looks up the three symbols, and fails cleanly if one is missing;
3. compares `engine_graphics_api_version()` with its own
   `plugin_api_version` (`<engine/graphics/PluginApi.hpp>`), and refuses the
   plugin if they differ;
4. calls `engine_graphics_create()`, and refuses the plugin if it returns
   null;
5. returns a `GraphicsPlugin` that owns the library and the backend, used only
   through the interfaces.

On failure it returns no plugin and an error naming the file; it never throws.
The path is used as is: resolving it relative to the executable is the
caller's job. When the `GraphicsPlugin` is destroyed, the backend is passed to
`engine_graphics_destroy()` before the library is closed.

Opening the library and looking up symbols is done by
`engine::core::DynamicLibrary` (`<engine/core/DynamicLibrary.hpp>`), which any
future plugin kind can reuse.

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

## graphics-raylib

| File | Role |
|---|---|
| `src/Plugin.cpp` | The three exported C functions |
| `src/RaylibBackend.hpp` | `IGraphicsBackend`: owns the renderer and the input |
| `src/RaylibRenderer.*` | Window, textures (a magenta checkerboard replaces a missing file), drawing in 1920 × 1080 virtual units |
| `src/Letterbox.*` | Scales the virtual screen to the window with black bars (static library, tested without Raylib) |
| `src/RaylibInput.*`, `src/KeyMap.*` | Engine `Key` ↔ Raylib key codes (tested without Raylib) |
| `src/TextureTable.hpp` | Texture ids, placeholder for missing files |

The window is resizable; `Esc` does not close it (`SetExitKey(KEY_NULL)`), so
the key stays free for the game.

## audio-raylib

The audio contract mirrors the graphics one:

```cpp
extern "C" std::uint32_t engine_audio_api_version();   // engine::audio::plugin_api_version
extern "C" engine::audio::IAudio* engine_audio_create();  // must not open the device
extern "C" void engine_audio_destroy(engine::audio::IAudio*);
```

`engine::audio::load_audio_plugin(path)` (`<engine/audio/PluginLoader.hpp>`)
checks them in the same order and returns an `AudioPlugin`. The caller then
calls `IAudio::open()`; when it returns `false` (no audio device), the backend
stays silent and every load returns the default id. When the plugin cannot be
loaded at all, the client uses `engine::audio::NullAudio`, which plays nothing.

| File | Role |
|---|---|
| `src/Plugin.cpp` | The three exported C functions |
| `src/RaylibAudio.*` | `IAudio` over Raylib's audio: `InitAudioDevice`, `LoadSound` + 3 `LoadSoundAlias` voices per sound so shots overlap, `LoadMusicStream` fed by `update()` |
| `src/AudioMixer.*` | Volume math (call × effects or music × master, clamped) and voice round-robin (static library, tested without Raylib) |
| `src/HandleTable.hpp` | Sound and music ids |

Raylib is linked statically into both plugins. Each plugin is opened with
`RTLD_LOCAL`, so each binds to its own copy; Raylib's audio does not need a
window, so the two copies never share state.

## Rules

- The backend library is included only inside its plugin: `raylib.h` appears
  nowhere else. This also avoids the `windows.h` / Raylib name clashes.
- All graphics calls happen on the main thread.
- Timing comes from the engine clock, not from the backend.
- The client draws in virtual units (1920 x 1080); the plugin handles scaling
  and letterboxing.
- Plugin loading is tested for both kinds: missing library, missing symbol,
  wrong API version, null backend, and the real Raylib plugin loaded without
  opening a window or an audio device.

## Adding a plugin

1. Create `plugins/<name>/` with its `CMakeLists.txt` calling
   `engine_add_plugin`, and add it to `plugins/CMakeLists.txt`.
2. Export the three C functions with `ENGINE_GRAPHICS_PLUGIN_EXPORT`
   (`<engine/graphics/PluginApi.hpp>`) or `ENGINE_AUDIO_PLUGIN_EXPORT`
   (`<engine/audio/PluginApi.hpp>`) and implement the engine interfaces.
3. Add the backend library to `vcpkg.json` and link it with
   `PRIVATE_DEPENDS`.
4. Add `add_dependencies(<executable> <name>)` in the executable that loads
   it, so it is built with it.
