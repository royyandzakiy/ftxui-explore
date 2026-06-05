# FTXUI Explore

Exploring usage of FTX UI

Successfully run on:

- Ubuntu 24.04, GCC
- Windows 11, Clang-Cl, MSVC Cl

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

# Run
bin\msvc\Debug\ftxui-explore.exe
```

Replace `msvc-debug` with any preset from `CMakePresets.json`.
