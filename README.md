# RType

## Build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`ninja-debug` / `ninja-release` presets use Ninja (on Windows, run them from a
Developer Command Prompt). CI runs `cmake --workflow --preset ci`.

## Checks (Linux / macOS)

```sh
cmake --workflow --preset asan       # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --workflow --preset tidy       # clang-tidy
```
