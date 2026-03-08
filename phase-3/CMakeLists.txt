cmake_minimum_required(VERSION 3.20)
project(mini1_phase3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# OpenMP (Homebrew on macOS).
if(APPLE)
    set(OpenMP_C_FLAGS   "-fopenmp")
    set(OpenMP_CXX_FLAGS "-fopenmp")
    set(OpenMP_C_LIB_NAMES   "omp")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
endif()
find_package(OpenMP REQUIRED)

# Phase 3 library (parallel SoA).
add_library(mini1_phase3 STATIC
    src/csv_parse.cpp
    src/columnar_store.cpp
    src/columnar_csv_reader.cpp
    src/columnar_query_engine.cpp
)
target_include_directories(mini1_phase3
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<$<BOOL:${APPLE}>:/opt/homebrew/opt/libomp/include>
)
target_link_libraries(mini1_phase3 PUBLIC OpenMP::OpenMP_CXX)

target_compile_options(mini1_phase3
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# Benchmark.
add_executable(benchmark_phase3 apps/benchmark_phase3.cpp)
target_link_libraries(benchmark_phase3 PRIVATE mini1_phase3)
