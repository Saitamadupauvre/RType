#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

readonly ci_clang_format_version="23.1.1"
readonly example_game="examples/r-type"

usage() {
    cat <<EOF
Usage: $0 <command>
       $0 editor [game-folder] [editor options]
       $0 play <mode> [runtime options]

  run        configure and build the release preset
  editor     build the release preset and open the editor
             on <game-folder> (default: examples/r-type)
  play       build the release preset and run examples/r-type with the runtime
             (play --solo, play --host <port>, play <server-ip> <port>,
             play --server <port>)
  debug      configure and build the debug preset
  tests      build the debug preset and run the tests
  tidy       build everything with clang-tidy (warnings as errors)
  format     format the sources in place
  all        format check, debug build, tests and clang-tidy
EOF
}

step() {
    printf '\n\033[1;34m==> %s\033[0m\n' "$1"
}

fail() {
    printf '\n\033[1;31mFAILED: %s\033[0m\n' "$1" >&2
    exit 1
}

build() {
    local preset="$1"
    step "Configure ($preset)"
    cmake --preset "$preset" || fail "configure $preset"
    step "Build ($preset)"
    cmake --build --preset "$preset" || fail "build $preset"
}

tests() {
    build debug
    step "Tests (debug)"
    ctest --preset debug || fail "tests"
}

tidy() {
    step "clang-tidy"
    cmake --workflow --preset tidy || fail "clang-tidy"
}

format() {
    step "Configure (debug)"
    cmake --preset debug || fail "configure debug"
    step "Format"
    cmake --build --preset debug --target format || fail "format"
}

format_check() {
    step "Configure (debug)"
    cmake --preset debug || fail "configure debug"
    step "Format check"
    local version
    version="$(clang-format --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)"
    if [[ "$version" != "$ci_clang_format_version" ]]; then
        printf 'warning: clang-format %s, CI uses %s (pipx install clang-format==%s)\n' \
            "$version" "$ci_clang_format_version" "$ci_clang_format_version" >&2
    fi
    cmake --build --preset debug --target format-check || fail "format check (run $0 format)"
}

launch() {
    local executable="$1"
    shift
    build release
    step "Start $executable $*"
    exec "./build/release/bin/$executable" "$@"
}

if [[ $# -lt 1 ]]; then
    usage >&2
    exit 2
fi

if [[ "$1" != editor && "$1" != play && $# -ne 1 ]]; then
    usage >&2
    exit 2
fi

if [[ "$1" == play && $# -lt 2 ]]; then
    usage >&2
    exit 2
fi

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    fail "VCPKG_ROOT is not set (see the README)"
fi

export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"

case "$1" in
    run) build release ;;
    editor) launch editor "${2:-$example_game}" "${@:3}" ;;
    play) launch engine-runtime --game "$example_game" "${@:2}" ;;
    debug) build debug ;;
    tests) tests ;;
    tidy) tidy ;;
    format) format ;;
    all)
        format_check
        tests
        tidy
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

printf '\n\033[1;32mDone: %s\033[0m\n' "$1"
