cmake_minimum_required(VERSION 3.20.1)

# Project Name and Version
project(CyclopsMIPS VERSION 0.1.0 LANGUAGES CXX)

# Fix for older third_party CMake dependencies (like Glad)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Standard C++ settings (C++20 is recommended for Systems work)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Enforce strict ISO C++

# Output directories (keeps your bin folder clean)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add subdirectories
add_subdirectory(Core)
add_subdirectory(App)

# Phase 7: Test & Benchmark suite
enable_testing()
add_subdirectory(Tests)