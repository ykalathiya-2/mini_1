cmake_minimum_required(VERSION 3.20)
project(mini1_memory_overload LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mini1_phase1
    src/csv_reader.cpp
    src/row_store.cpp
    src/query_engine.cpp
    src/data_facade.cpp
)

target_include_directories(mini1_phase1
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(mini1_phase1
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(benchmark_phase1
    apps/benchmark_phase1.cpp
)

target_link_libraries(benchmark_phase1
    PRIVATE
        mini1_phase1
)
