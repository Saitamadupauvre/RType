# CMake Commands Reference

Quick reference for common CMake operations in this project.

## Prerequisites

Set `VCPKG_ROOT` environment variable pointing to your vcpkg installation:
```sh
export VCPKG_ROOT=/path/to/vcpkg
```

## Build Commands

### Configure project
```sh
cmake --preset ninja-debug
```
Creates build configuration in `build/ninja-debug/`.

### Build all targets
```sh
cmake --build --preset ninja-debug
```

### Build specific target
```sh
cmake --build --preset ninja-debug --target r-type_server
cmake --build --preset ninja-debug --target engine-physics
```

### Clean build
```sh
rm -rf build/ninja-debug
cmake --preset ninja-debug
cmake --build --preset ninja-debug
```

## Test Commands

### Run all tests
```sh
ctest --preset ninja-debug
```

### Run specific test suite
```sh
ctest --preset ninja-debug -R PhysicsSystem
ctest --preset ninja-debug -R Collisions
ctest --preset ninja-debug -R EntityManager
```

### Run tests with verbose output
```sh
ctest --preset ninja-debug --output-on-failure
ctest --preset ninja-debug -V
```

### Run single test executable directly
```sh
./build/ninja-debug/tests/engine_physics_tests
./build/ninja-debug/tests/engine_core_tests
```

## Code Quality

### Format code
```sh
cmake --build --preset ninja-debug --target format
```
Applies `clang-format` to all source files.

### Run clang-tidy
```sh
cmake --workflow --preset tidy
```
Static analysis with clang-tidy.

### Run sanitizers
```sh
cmake --workflow --preset asan
```
AddressSanitizer + UndefinedBehaviorSanitizer.

## Workflows (CI-equivalent)

### Run full CI pipeline locally
```sh
cmake --workflow --preset ci
```
Configure → Build → Test → Format check.

### Available workflows
- `ci`: Full CI (Linux/macOS checks)
- `asan`: AddressSanitizer + UBSan
- `tidy`: clang-tidy static analysis

## Common Patterns

### After modifying code
```sh
cmake --build --preset ninja-debug
ctest --preset ninja-debug
```

### Before committing
```sh
cmake --build --preset ninja-debug --target format
ctest --preset ninja-debug
```

### After adding new dependencies (vcpkg.json)
```sh
rm -rf build/ninja-debug/vcpkg_installed
cmake --preset ninja-debug
cmake --build --preset ninja-debug
```

### After changing CMakeLists.txt
```sh
cmake --preset ninja-debug
cmake --build --preset ninja-debug
```

## Presets Available

Check `.cmake/presets/` for all available presets. Common ones:
- `ninja-debug`: Debug build with Ninja generator
- `ninja-release`: Release build with optimizations
- `ci`: CI pipeline workflow
- `asan`: Sanitizer workflow
- `tidy`: Static analysis workflow
