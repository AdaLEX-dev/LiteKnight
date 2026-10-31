# Building LiteKnight

## Prerequisites

- Linux + Wayland
- CMake ≥ 3.25, Ninja
- C++20 compiler
- SDL3 ≥ 3.2, Vulkan loader/headers, libpng, fmt, glslc
- Python 3 for inventory/audit scripts
- `rsync` for local asset preparation

The reference APK/bundle tree is not required to compile, run unit tests, or run
metadata audits. It is required to generate the recovered devroom pixels
locally.

## Standard development build

```bash
./scripts/build-dev.sh
```

Defaults:

```text
build directory: build
build type:      RelWithDebInfo
jobs:            nproc
```

Override without editing the script:

```bash
LITEKNIGHT_BUILD_DIR=build-dev LITEKNIGHT_BUILD_TYPE=Debug ./scripts/build-dev.sh
```

## Manual build

```bash
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"
./scripts/prepare-assets.sh
```

## Debug / sanitizers

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j"$(nproc)"
ctest --test-dir build-debug --output-on-failure
```

Debug flags include AddressSanitizer and UndefinedBehaviorSanitizer.

## Clean rebuild

```bash
rm -rf build build-debug
./scripts/build-dev.sh
./scripts/test.sh
```

Never commit build directories; they are gitignored.
