# RType

## Requirements

- CMake 3.25+, a C++20 compiler (GCC, Clang or MSVC), Ninja (optional).
- [vcpkg](https://github.com/microsoft/vcpkg): every dependency comes from
  `vcpkg.json` and is built automatically on the first configure.

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh              # bootstrap-vcpkg.bat on Windows
export VCPKG_ROOT=~/vcpkg               # add it to your shell profile
```

## Build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`ninja-debug` / `ninja-release` presets use Ninja (on Windows, run them from a
Developer Command Prompt). CI runs `cmake --workflow --preset ci`.

Output goes to `build/<preset>/bin/`:

```sh
./build/ninja-debug/bin/r-type_server <port>
./build/ninja-debug/bin/r-type_client <server-ip> <port>
```

## Checks (Linux / macOS)

```sh
cmake --workflow --preset asan       # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --workflow --preset tidy       # clang-tidy
```
