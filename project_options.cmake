# Compiler sanitizers
option(SETUP_VCPKG "Check & setup vcpkg installation" ON)
option(ENABLE_STRICT_COMPILER "Strict compiler options, sees warnings as errors!" ON)

option(ENABLE_SANITIZERS "Enable static & runtime sanitizers" OFF)
option(ENABLE_ASAN "Enable Address, Leak, and Undefined sanitizers" OFF)
option(ENABLE_TSAN_MSAN "Enable Thread sanitizer (Data races) & Memory sanitizer (Uninitialized reads)" OFF)
option(ENABLE_CLANG_TIDY "Enable clang tidy" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck" OFF)
option(ENABLE_CCACHE "Enable cache" OFF)
option(ENABLE_TRACY "Enable tracy" OFF)
