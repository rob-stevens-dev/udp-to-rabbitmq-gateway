cmake_minimum_required(VERSION 3.16)

# Find GTest
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Unit tests
add_subdirectory(unit)

# Fuzz tests
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()

# Benchmark tests
if(ENABLE_BENCHMARKING)
    add_subdirectory(benchmark)
endif()