cmake_minimum_required(VERSION 3.20)

# -----------------------------------------------------------------------
# Google Test — Consolidated to root third_party/googletest
# -----------------------------------------------------------------------
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)    # Link against shared CRT (MSVC /MD)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)

# Point to the root-level third_party directory
add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/googletest" "${CMAKE_BINARY_DIR}/googletest_build" EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------
# Google Benchmark — Consolidated to root third_party/benchmark
# -----------------------------------------------------------------------
set(BENCHMARK_ENABLE_TESTING  OFF CACHE BOOL "" FORCE)   # Don't run benchmark's own tests
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL  OFF CACHE BOOL "" FORCE)
set(BENCHMARK_USE_BUNDLED_GTEST ON  CACHE BOOL "" FORCE)

# Point to the root-level third_party directory
add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/benchmark" "${CMAKE_BINARY_DIR}/benchmark_build" EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------
# Suppress strict-warning flags for third-party code
# -----------------------------------------------------------------------
# (googletest and benchmark are not our code; we don't want WX on them)

# -----------------------------------------------------------------------
# CyclopsTests — Google Test Layered Suite (V1.0 Hardening)
# -----------------------------------------------------------------------
add_executable(CyclopsTests
    # Layer 1: Component Unit Tests
    test_alu.cpp
    Test_RegFile.cpp
    Test_Instruction.cpp
    
    # Layer 2: Pipeline Integration Tests
    Test_Hazards.cpp
    
    # Layer 3: Golden Master (E2E) Tests
    Test_GoldenMaster.cpp

    # Legacy / Utility
    test_assembler.cpp
    test_pipeline_mapping.cpp
)

target_link_libraries(CyclopsTests
    PRIVATE
        CyclopsCore
        GTest::gtest_main
)

target_compile_options(CyclopsTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
)

include(GoogleTest)
gtest_discover_tests(CyclopsTests)

# -----------------------------------------------------------------------
# CyclopsBench — Google Benchmark executable
# -----------------------------------------------------------------------
add_executable(CyclopsBench
    bench_pipeline.cpp
)

target_link_libraries(CyclopsBench
    PRIVATE
        CyclopsCore
        benchmark::benchmark_main
)

target_compile_options(CyclopsBench PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
)
