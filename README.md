# mini\_1 — Memory Overload

Performance study on the 2017 Yellow Taxi Trip dataset (113M rows, ~14.5 GB).
Built in three phases: serial baseline, OpenMP parallel, and columnar (SoA) optimized.

## Layout

| Directory | What it does |
|-----------|--------------|
| `phase-1/` | Serial C++ library + benchmark (AoS, virtual interfaces, templates, facade) |
| `phase-2/` | OpenMP parallel CSV reader + parallel query engine, speedup measurement |
| `phase-3/` | Columnar (SoA) layout, zero-alloc parallel loader, columnar query engine |

## Quick start

### Phase 1 (serial)

```bash
cd phase-1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv
```

### Phase 2 (parallel, needs Homebrew LLVM + libomp on macOS)

```bash
cd phase-2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@20/bin/clang++
cmake --build build -j
./build/benchmark_phase2 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv
```

### Phase 3 (columnar SoA, needs Homebrew LLVM + libomp on macOS)

```bash
cd phase-3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@20/bin/clang++
cmake --build build -j
./build/benchmark_phase3 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv
```

## Dataset

NYC 2017 Yellow Taxi Trip Data — 17 columns, ~113.5M rows.
Download from the NYC Open Data portal.

## Requirements

- C++20 compiler (AppleClang 17+ for Phase 1, LLVM clang++ 20 for Phase 2–3)
- CMake 3.20+
- OpenMP (`brew install libomp llvm@20` on macOS)
- Python 3.10+ with pandas, matplotlib, seaborn (for plotting)

## Notes

- Memory is measured via `task_vm_info.phys_footprint` (macOS).
- Phase-specific docs live in each directory's own README.
