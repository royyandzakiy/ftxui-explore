# C++ Project Template

A cross-platform C++23 project template with CMake Presets, vcpkg, sanitizers, static analysis, and CI out of the box.

Targets Windows (MSVC, Clang-CL, MinGW) and Linux (GCC, Clang). Requires CMake 3.21+ and vcpkg.

---

## Prerequisites

- CMake 3.21+
- vcpkg (set `VCPKG_ROOT` env variable, or configure `cmake/local_vcpkg.cmake`)
- A C++23-capable compiler

---

## Quick Start

```bash
# Configure
cmake --preset msvc-debug

# Build
cmake --build build/msvc-debug

# Run tests
ctest --preset test-msvc-debug
```

Replace `msvc-debug` with any preset from `CMakePresets.json`.

---

## Project Structure

```
.
├── cmake/                    # CMake utility modules
│   ├── analyzers_optimizers.cmake
│   ├── config_vcpkg.cmake
│   ├── version.cmake
│   ├── version.h.in
│   └── local_vcpkg.cmake.example
├── include/cmake_project_template/
│   └── version.h             # Auto-generated from version.txt
├── src/                      # Application sources
├── test/                     # GoogleTest unit tests
├── .github/workflows/        # CI pipeline
├── CMakeLists.txt
├── CMakePresets.json
├── project_options.cmake     # Feature toggles
├── vcpkg.json                # Package manifest
└── version.txt               # Project version (1.0.0)
```

---

## CMake Presets

Presets are defined in `CMakePresets.json` and cover all supported toolchains:

| Preset                                | Platform | Compiler         |
| ------------------------------------- | -------- | ---------------- |
| `msvc-debug` / `msvc-release`         | Windows  | MSVC (VS 2022)   |
| `clang-cl-debug` / `clang-cl-release` | Windows  | Clang-CL + Ninja |
| `mingw-debug` / `mingw-release`       | Windows  | MinGW GCC        |
| `clang-linux-debug`                   | Linux    | Clang 17         |
| `gcc-linux-debug`                     | Linux    | GCC 13           |

Each preset sets the generator, compiler, output directory under `bin/`, and vcpkg triplet. Test presets (`test-*`) are paired with each configure preset.

---

## Feature Toggles (`project_options.cmake`)

| Option                   | Default | Description                                             |
| ------------------------ | ------- | ------------------------------------------------------- |
| `SETUP_VCPKG`            | ON      | Validate vcpkg installation on configure                |
| `ENABLE_STRICT_COMPILER` | ON      | Warnings as errors (`/W4 /WX`, `-Wall -Wextra -Werror`) |
| `ENABLE_SANITIZERS`      | OFF     | UBSan + bounds + integer checks                         |
| `ENABLE_ASAN`            | OFF     | AddressSanitizer + LeakSanitizer                        |
| `ENABLE_TSAN_MSAN`       | OFF     | ThreadSanitizer or MemorySanitizer                      |
| `ENABLE_CLANG_TIDY`      | OFF     | Clang-Tidy static analysis                              |
| `ENABLE_CPPCHECK`        | OFF     | Cppcheck static analysis                                |
| `ENABLE_CCACHE`          | OFF     | ccache compiler launcher                                |
| `ENABLE_TRACY`           | OFF     | Tracy profiler (v0.13.1 via FetchContent)               |

Pass options at configure time:

```bash
cmake --preset clang-linux-debug -DENABLE_ASAN=ON -DENABLE_CLANG_TIDY=ON
```

---

## cmake/ Modules

**`config_vcpkg.cmake`** -- Locates vcpkg from `VCPKG_ROOT`, env variable, or a local subdirectory. Validates the toolchain file and reports installed packages. Copy `local_vcpkg.cmake.example` to `local_vcpkg.cmake` to override the vcpkg path locally without touching tracked files.

**`analyzers_optimizers.cmake`** -- Wires up sanitizers, Clang-Tidy, Cppcheck, ccache, and Tracy based on the options in `project_options.cmake`. All analysis tools are configured per-target.

**`version.cmake`** -- Reads `version.txt`, splits into major/minor/patch, and generates `include/cmake_project_template/version.h` from `version.h.in`.

---

## Sanitizers and Static Analysis

Sanitizers are Clang/GCC only. On Windows, use a `clang-*` or `mingw-*` preset.

- **`ENABLE_SANITIZERS`** -- adds `-fsanitize=undefined,bounds,integer`
- **`ENABLE_ASAN`** -- adds `-fsanitize=address` with leak detection
- **`ENABLE_TSAN_MSAN`** -- thread or memory sanitizer (mutually exclusive with ASan)

`src/main_fail_sanitizer.cpp` contains intentional violations for verifying sanitizer output.

**Clang-Tidy** rules are in `.clang-tidy` (bugprone, modernize, performance, cppcoreguidelines; warnings as errors). **Cppcheck** runs as a CMake target-level check. Code style is enforced by `.clang-format` (Microsoft base, 120-char line limit, tabs).

---

## Dependencies (`vcpkg.json`)

| Package | Use                                              |
| ------- | ------------------------------------------------ |
| `fmt`   | String formatting                                |
| `gtest` | Unit testing (fetched via FetchContent in tests) |
| `tracy` | Performance profiling (optional, FetchContent)   |

---

## CI (`.github/workflows/`)

Runs on push to `main`/`master` and pull requests. Matrix builds on `ubuntu-latest` with GCC 13 and Clang 17. Steps: checkout, install tools, bootstrap vcpkg, configure, build, run. Build artifacts are uploaded with a 7-day retention window.

---

## Adapting the Template

1. Rename the project in `CMakeLists.txt` and `vcpkg.json`.
2. Update `version.txt`.
3. Replace sources under `src/` and headers under `include/`.
4. Add vcpkg dependencies to `vcpkg.json`.
5. Adjust presets in `CMakePresets.json` as needed.
