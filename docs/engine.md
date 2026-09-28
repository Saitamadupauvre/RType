# Engine modules

The engine lives in `engine/` and is split into modules. Each module is a
shared library (`libengine-<name>.so` / `engine-<name>.dll`) created with the
`engine_add_module` CMake helper from `engine/cmake/EngineModule.cmake`.

## The contract

A module promises three things to the code that uses it:

1. **A stable target name**: `engine::<name>`. Nobody links `engine-<name>`
   directly. When the engine moves to its own repository, the same name comes
   from `find_package(engine)` and the game does not change.
2. **A public API made only of engine types**: headers in
   `engine/include/engine/<name>/`, with every public symbol marked
   `ENGINE_<NAME>_EXPORT`. No third-party header (Lua, Asio, Raylib...) ever
   appears there.
3. **Nothing else**: every symbol not marked with the export macro is hidden,
   and everything in `engine/src/<name>/` is private.

In return, a module may only depend on:

- other engine modules (`DEPENDS`, linked PUBLIC);
- third-party libraries from vcpkg (`PRIVATE_DEPENDS`, linked PRIVATE).

It never depends on the game or on a plugin.

## Declaring a module

```cmake
engine_add_module(script
    SOURCES src/script/Module.cpp
    DEPENDS engine::core
    PRIVATE_DEPENDS sol2::sol2
)
```

| Argument | Meaning |
|---|---|
| `script` | Module name: target `engine-script`, alias `engine::script`. |
| `SOURCES` | `.cpp` files, relative to `engine/`. |
| `DEPENDS` | Engine modules. PUBLIC: users of `engine::script` also get them. |
| `PRIVATE_DEPENDS` | Third-party libraries. PRIVATE: their headers stay inside the module. |

What the helper does:

| Step | Result |
|---|---|
| `add_library(engine-script SHARED ...)` | Shared library in `bin/`. |
| `add_library(engine::script ALIAS ...)` | Stable name for users. |
| `generate_export_header` | `<engine/script/Export.hpp>` defining `ENGINE_SCRIPT_EXPORT`. |
| `CXX_VISIBILITY_PRESET hidden` | Symbols hidden by default, on Linux as on Windows. |
| Include directories | `engine/include/` and the generated folder are public. |
| `engine_set_warnings` | Engine warning flags, errors when `ENGINE_WARNINGS_AS_ERRORS` is on. |
| `install(TARGETS ...)` | Installed into `bin/`, next to the executables. |

## The export macro

`ENGINE_SCRIPT_EXPORT` expands differently depending on who compiles:

| Compiled as part of | Linux / macOS | Windows |
|---|---|---|
| `engine-script` itself | `__attribute__((visibility("default")))` | `__declspec(dllexport)` |
| Anything using it | `__attribute__((visibility("default")))` | `__declspec(dllimport)` |

```cpp
#include "engine/script/Export.hpp"

namespace engine::script {

ENGINE_SCRIPT_EXPORT void load_game(std::string_view folder);

class ENGINE_SCRIPT_EXPORT LuaRuntime {
public:
    void tick(float dt);
};

void internal_helper();

} // namespace engine::script
```

`load_game` and `LuaRuntime` are visible outside the library; `internal_helper`
is not. Because symbols are hidden on every platform, forgetting the macro
fails on Linux too, not only on the Windows CI.

## Rules

- Public headers include only the standard library, engine headers and
  `Export.hpp`.
- No global state in headers: registries live in a `.cpp` of `engine-core`
  behind exported functions.
- `engine/` uses only its own CMake helpers and variables, never root ones, so
  it stays extractable.
- A module talks to another through the ECS and the event bus, not by calling
  it directly.

## Adding a module

1. Create `engine/include/engine/<name>/` and `engine/src/<name>/`.
2. Add `engine_add_module(<name> SOURCES ... DEPENDS engine::core)` to
   `engine/CMakeLists.txt`.
3. Add tests in `tests/engine/<name>/` and register them with
   `engine_add_test(<name> SOURCES ...)` in `tests/CMakeLists.txt`.
4. Add any new third-party library to `vcpkg.json` and link it with
   `PRIVATE_DEPENDS`.
5. Link `engine::<name>` from `game/CMakeLists.txt` where needed.
