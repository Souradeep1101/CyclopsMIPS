cmake_minimum_required(VERSION 3.20)

# -----------------------------------------------------------------------
# Google Test — via git submodule at Tests/third_party/googletest
# -----------------------------------------------------------------------
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)    # Link against shared CRT (MSVC /MD)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------
# Google Benchmark — via git submodule at Tests/third_party/benchmark
# -----------------------------------------------------------------------
set(BENCHMARK_ENABLE_TESTING  OFF CACHE BOOL "" FORCE)   # Don't run benchmark's own tests
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL  OFF CACHE BOOL "" FORCE)
set(BENCHMARK_USE_BUNDLED_GTEST ON  CACHE BOOL "" FORCE)
add_subdirectory(third_party/benchmark EXCLUDE_FROM_ALL)

# -----------------------------------------------------------------------
# Suppress strict-warning flags for third-party code
# -----------------------------------------------------------------------
# (googletest and benchmark are not our code; we don't want WX on them)

# -----------------------------------------------------------------------
# CyclopsTests — Google Test executable
# -----------------------------------------------------------------------
add_executable(CyclopsTests
    test_assembler.cpp
    test_cpu.cpp
    test_alu.cpp
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
