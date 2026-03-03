# mini_1 — Memory Overload (Phase 1)

Serial C++ library + benchmark for the 2017 Yellow Taxi Trip dataset.

## What's here

- CSV parser that loads all 17 columns into row-based (`TaxiTrip`) structs
- Virtual interfaces (`IDataReader`, `IDataStore`, `IQueryEngine`) for extensibility
- Template `range_scan<>` for generic numeric field queries
- `DataFacade` that wires everything together
- Benchmark executable with CPU / memory / timing instrumentation
- Python plotting pipeline (seaborn SVG charts + HTML report)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run benchmark

```bash
# defaults: trip_distance in [1, 3], 10 repeats
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv

# custom query
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv fare_amount 10 50 10

# machine-readable CSV row
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv trip_distance 1 3 10 --csv
```

## Collect multiple runs

```bash
./scripts/run_phase1_benchmark.sh ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv 10 trip_distance 1 3
```

## Full pipeline (benchmark + plots + report)

```bash
./scripts/run_phase1_pipeline.sh ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv 10 trip_distance 1 3 phase1_report
```

## Generate graphs separately

```bash
pip install -r scripts/requirements.txt
python3 scripts/plot_phase1_benchmark.py --input phase1_results.csv --outdir phase1_report
```

Outputs under `phase1_report/`: summary_stats.csv, time_trends.svg, cpu_trends.svg, memory_trend.svg, distributions.svg, report.html.

## Notes

- Phase 1 is serial only — no threads or OpenMP.
- Memory reporting uses `task_vm_info.phys_footprint` on macOS (matches Activity Monitor).
