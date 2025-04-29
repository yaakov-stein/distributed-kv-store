cmake_minimum_required(VERSION 3.16)
project(EmulationTests LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# Find GoogleTest (if installed via Homebrew)
find_package(GTest REQUIRED)

# Enable testing
enable_testing()

# Build the test executable
add_executable(EmulationTest
    src/Util.hpp
    src/Message/NoopMessage.cpp
    src/Emulation/Emulation.cpp
    tests/EmulationTest.cpp
)

# Manually set paths if GTest is installed via Homebrew
find_path(GTEST_INCLUDE_DIR NAMES gtest/gtest.h
  PATHS /opt/homebrew/Cellar/googletest/1.16.0/include/gtest
)

find_library(GTEST_LIBRARY NAMES gtest
  PATHS /opt/homebrew/Cellar/googletest/1.16.0/lib
)

find_library(GTEST_MAIN_LIBRARY NAMES gtest_main
  PATHS /opt/homebrew/Cellar/googletest/1.16.0/lib
)

# Ensure the include directory is added
include_directories(${GTEST_INCLUDE_DIR})

# Link the GoogleTest libraries
target_link_libraries(EmulationTest 
    ${GTEST_LIBRARY} 
    ${GTEST_MAIN_LIBRARY} 
    pthread
)

# Auto-discover tests
include(GoogleTest)
gtest_discover_tests(EmulationTest)
