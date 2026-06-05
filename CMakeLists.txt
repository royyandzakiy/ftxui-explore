cmake_minimum_required(VERSION 3.5)

include(project_options.cmake)
include(cmake/config_vcpkg.cmake)

#==============================================================================
# Project Configuration
#==============================================================================
project(
  ftxui-explore
  LANGUAGES CXX
  VERSION 1.0.0)

#==============================================================================
# Compiler Configuration
#==============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ENABLE_STRICT_COMPILER)
  if(MSVC)
    add_compile_options(/W4 /permissive- /WX)
  else()
    # Clang/GCC
    add_compile_options(
      -Wall
      -Wextra
      -Werror
      -pedantic)
  endif()
endif()

include(cmake/analyzers_optimizers.cmake)

#==============================================================================
# Dependencies
#==============================================================================
find_package(fmt CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG main)
FetchContent_MakeAvailable(ftxui)

#==============================================================================
# Project Structure
#==============================================================================

#==============================================================================
# Main Executable
#==============================================================================
add_executable(${PROJECT_NAME} src/main_form_input.cpp)
# add_executable(${PROJECT_NAME} src/main_counter.cpp)
# add_executable(${PROJECT_NAME} src/main_4_clicker.cpp)

# Core dependencies (always included)
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE fmt::fmt
          ftxui::dom
          ftxui::screen
          ftxui::component)
