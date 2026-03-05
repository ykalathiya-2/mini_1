cmake_minimum_required(VERSION 3.20)
project(mini1_phase2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# OpenMP — requires Homebrew LLVM clang++ on macOS.
# Configure with:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm@20/bin/clang \
#     -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@20/bin/clang++
if(APPLE)
    set(OpenMP_C_FLAGS   "-fopenmp")
    set(OpenMP_CXX_FLAGS "-fopenmp")
    set(OpenMP_C_LIB_NAMES   "omp")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
endif()
find_package(OpenMP REQUIRED)

# Pull in Phase 1's library and headers.
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../phase-1
    ${CMAKE_BINARY_DIR}/phase1
)

add_library(mini1_phase2
    src/parallel_csv_reader.cpp
    src/parallel_query_engine.cpp
)

target_include_directories(mini1_phase2
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<$<BOOL:${APPLE}>:/opt/homebrew/opt/libomp/include>
)

target_link_libraries(mini1_phase2
    PUBLIC
        mini1_phase1
        OpenMP::OpenMP_CXX
)

target_compile_options(mini1_phase2
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(benchmark_phase2
    apps/benchmark_phase2.cpp
)

target_link_libraries(benchmark_phase2
    PRIVATE
        mini1_phase2
)
