cmake_minimum_required(VERSION 3.16)

# Worldseed Engine — ISO C17 build.
# CMake is a development tool only; it is not a product runtime dependency.
project(worldseed
        VERSION 0.20.0
        DESCRIPTION "Worldseed Engine — living digital ecosystems"
        LANGUAGES C)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 17)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(WorldseedFlags)

option(WS_BUILD_TESTS "Build unit tests" ON)
option(WS_BUILD_TOOLS "Build developer tools" ON)
option(WS_ENABLE_SANITIZERS "Enable ASan/UBSan where available" OFF)
option(WS_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

ws_apply_common_flags()

add_subdirectory(src)
add_subdirectory(src/app)
if(WS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(WS_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

message(STATUS "Worldseed: C standard=${CMAKE_C_STANDARD} build=${CMAKE_BUILD_TYPE} "
               "compiler=${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
