# cmake/config_vcpkg.cmake
# =============================================================================
# VCPKG Configuration
# =============================================================================
if(NOT SETUP_VCPKG)
  message(STATUS "Skip vcpkg installation check & setup")
  return()
endif()

# Allow local override (gitignored)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/local_vcpkg.cmake")
  message(STATUS "Loading local vcpkg configuration")
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/local_vcpkg.cmake)
endif()

# Set triplet
if(NOT VCPKG_TARGET_TRIPLET)
  if(WIN32)
    set(VCPKG_TARGET_TRIPLET
        "x64-windows"
        CACHE STRING "vcpkg triplet")
  else()
    set(VCPKG_TARGET_TRIPLET
        "x64-linux"
        CACHE STRING "vcpkg triplet")
  endif()
endif()

# Find vcpkg root (priority: CMake var > Env var > Project subdir > Error)
if(NOT VCPKG_ROOT_PATH)
  if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT_PATH "$ENV{VCPKG_ROOT}")
  elseif(DEFINED ENV{VCPKG_ROOT_PATH})
    set(VCPKG_ROOT_PATH "$ENV{VCPKG_ROOT_PATH}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
    set(VCPKG_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
  else()
    message(
      FATAL_ERROR
        "VCPKG_ROOT_PATH not set. Set it via:\n"
        "  - cmake -DVCPKG_ROOT_PATH=/path/to/vcpkg\n"
        "  - export VCPKG_ROOT=/path/to/vcpkg\n"
        "  - Create cmake/local_vcpkg.cmake file")
  endif()
endif()

# Validate VCPKG_ROOT_PATH
if(NOT DEFINED VCPKG_ROOT_PATH)
  message(
    FATAL_ERROR
      "\n╔════════════════════════════════════════════════════════════════════════════╗\n"
      "║  VCPKG_ROOT_PATH is not set!                                               ║\n"
      "║  Please define it in your CMake preset or environment, ways include:		  ║\n"
      "║    - File: Create cmake/local_vcpkg.cmake file							  ║\n"
      "║    - Build: cmake -DVCPKG_ROOT_PATH=/path/to/vcpkg						  ║\n"
      "║    - Environment: export VCPKG_ROOT_PATH=/path/to/vcpkg                    ║\n"
      "╚════════════════════════════════════════════════════════════════════════════╝\n")
endif()

# Set toolchain file
if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
      "${VCPKG_ROOT_PATH}/scripts/buildsystems/vcpkg.cmake"
      CACHE FILEPATH "vcpkg toolchain file")
endif()

# Status indicators
set(STATUS_OK "✓")
set(STATUS_FAIL "✗")
set(STATUS_WARN "⚠")
set(STATUS_INFO "ℹ")

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║                           VCPKG CONFIGURATION                              ║")
message(STATUS "╚════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")

# Initialize counters
set(VCPKG_CHECKS_PASSED 0)
set(VCPKG_CHECKS_TOTAL 3)

# -----------------------------------------------------------------------------
# Check 1: CMAKE_TOOLCHAIN_FILE
# -----------------------------------------------------------------------------
message(STATUS "┌─ [1/${VCPKG_CHECKS_TOTAL}] CMAKE_TOOLCHAIN_FILE")
if(DEFINED CMAKE_TOOLCHAIN_FILE)
  if(EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(STATUS "│   ${STATUS_OK} ${CMAKE_TOOLCHAIN_FILE}")
    math(EXPR VCPKG_CHECKS_PASSED "${VCPKG_CHECKS_PASSED} + 1")
  else()
    message(STATUS "│   ${STATUS_FAIL} ${CMAKE_TOOLCHAIN_FILE} (file not found)")
    message(FATAL_ERROR "│\n╚════════════════════════════════════════════════════════════════════════════╝\n"
                        "CMAKE_TOOLCHAIN_FILE not found! Please verify vcpkg installation path.\n")
  endif()
else()
  message(STATUS "│   ${STATUS_FAIL} CMAKE_TOOLCHAIN_FILE is not set!")
  message(FATAL_ERROR "│\n╚════════════════════════════════════════════════════════════════════════════╝\n"
                      "CMAKE_TOOLCHAIN_FILE must be set! Add to your CMake preset.\n")
endif()
message(STATUS "└─────────────────────────────────────────────────────────────────────────")

# -----------------------------------------------------------------------------
# Check 2: VCPKG_TARGET_TRIPLET
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "┌─ [2/${VCPKG_CHECKS_TOTAL}] VCPKG_TARGET_TRIPLET")
if(DEFINED VCPKG_TARGET_TRIPLET)
  message(STATUS "│   ${STATUS_OK} ${VCPKG_TARGET_TRIPLET}")
  math(EXPR VCPKG_CHECKS_PASSED "${VCPKG_CHECKS_PASSED} + 1")
else()
  message(STATUS "│   ${STATUS_INFO} Using default: ${VCPKG_TARGET_TRIPLET}")
  set(VCPKG_TARGET_TRIPLET
      "${VCPKG_TARGET_TRIPLET}"
      CACHE STRING "vcpkg triplet" FORCE)
  math(EXPR VCPKG_CHECKS_PASSED "${VCPKG_CHECKS_PASSED} + 1")
endif()
message(STATUS "└─────────────────────────────────────────────────────────────────────────")

# -----------------------------------------------------------------------------
# Check 3: vcpkg installation
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "┌─ [3/${VCPKG_CHECKS_TOTAL}] vcpkg Installation")

# Detect Mode
if(VCPKG_MANIFEST_MODE)
  message(STATUS "│   ${STATUS_INFO} Mode: MANIFEST (using vcpkg.json)")
  message(STATUS "│   ${STATUS_OK} dependencies will be managed automatically")
  math(EXPR VCPKG_CHECKS_PASSED "${VCPKG_CHECKS_PASSED} + 1")
else()
  message(STATUS "│   ${STATUS_INFO} Mode: CLASSIC (global installation)")

  set(TRIPLET_DIR "${VCPKG_ROOT_PATH}/installed/${VCPKG_TARGET_TRIPLET}")
  if(EXISTS "${TRIPLET_DIR}")
    message(STATUS "│   ${STATUS_OK} triplet directory: ${TRIPLET_DIR}")
    math(EXPR VCPKG_CHECKS_PASSED "${VCPKG_CHECKS_PASSED} + 1")

    # Quick global package checks if using vcpkg classic mode. If using manifest mode, ignore
    if(EXISTS "${TRIPLET_DIR}/include/fmt")
      message(STATUS "│   ${STATUS_OK} fmt: found")
    else()
      message(STATUS "│   ${STATUS_WARN} fmt: not found (run: ./vcpkg install fmt --triplet ${VCPKG_TARGET_TRIPLET})")
    endif()
    if(EXISTS "${TRIPLET_DIR}/include/gtest")
      message(STATUS "│   ${STATUS_OK} gtest: found")
    else()
      message(
        STATUS "│   ${STATUS_WARN} gtest: not found (run: ./vcpkg install gtest --triplet ${VCPKG_TARGET_TRIPLET})")
    endif()

    # Add more as needed, following this format:
    # if(EXISTS "${TRIPLET_DIR}/include/drogon")
    #     message(STATUS "│   ${STATUS_OK} drogon: found")
    # else()
    #     message(STATUS "│   ${STATUS_WARN} drogon: not found (run: ./vcpkg install drogon --triplet ${VCPKG_TARGET_TRIPLET})")
    # endif()

  else()
    message(STATUS "│   ${STATUS_WARN} triplet directory not found: ${TRIPLET_DIR}")
    message(STATUS "│   ${STATUS_INFO} Run: ./vcpkg install --triplet ${VCPKG_TARGET_TRIPLET}")
    math(EXPR VCPKG_CHECKS_PASSED "${VCPKG_CHECKS_PASSED} + 0")
  endif()
endif()
message(STATUS "└─────────────────────────────────────────────────────────────────────────")

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║                         VCPKG CONFIGURATION SUMMARY                        ║")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════╣")
message(STATUS "║  ${STATUS_OK} Toolchain file       : ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "║  ${STATUS_OK} vcpkg root           : ${VCPKG_ROOT_PATH}")
message(STATUS "║  ${STATUS_OK} Triplet              : ${VCPKG_TARGET_TRIPLET}")
message(STATUS "╠════════════════════════════════════════════════════════════════════════════╣")
message(
  STATUS
    "║  Checks passed: ${VCPKG_CHECKS_PASSED}/${VCPKG_CHECKS_TOTAL} (${VCPKG_CHECKS_TOTAL} critical)				                            ║"
)
if(VCPKG_CHECKS_PASSED EQUAL VCPKG_CHECKS_TOTAL)
  message(STATUS "║  Status: ${STATUS_OK} READY for configuration                                        ║")
else()
  message(STATUS "║  Status: ${STATUS_WARN} Some checks failed - proceed with caution                      ║")
endif()
message(STATUS "╚════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")
