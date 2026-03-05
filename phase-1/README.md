# Phase 1 — Serial Baseline

## Purpose

Phase 1 establishes the **serial (single-threaded) baseline** for the Memory Overload project. The goal is to load the 2017 NYC Yellow Taxi dataset (~113 million rows, ~14.5 GB) into memory, support numeric range queries against any column, and collect precise performance metrics (wall time, CPU time, memory). Everything here is serial — no threads, no OpenMP — so that later phases have a clean baseline to compare against.

---

## Architecture Overview

The codebase is organized as an **object-oriented library** built on three abstract interfaces. Concrete implementations sit behind those interfaces and are wired together by a Facade. The benchmark executable uses only the Facade — it does not touch the concrete classes directly.

```
IDataReader        IDataStore         IQueryEngine
     |                  |                   |
  CsvReader          RowStore          QueryEngine
      \                 |                  /
       `----------  DataFacade  ----------'
                        |
               benchmark_phase1 (app)
```

This separation means that in Phase 2 and Phase 3 you can swap in a parallel reader or a vectorized store without changing how the benchmark or the facade work.

---

## File-by-File Reference

### `include/mini1/taxi_trip.hpp`

Defines `TaxiTrip`, the central data type — a plain C++ struct holding one row from the CSV.

```cpp
struct TaxiTrip {
    int32_t vendor_id;
    std::string pickup_datetime;
    std::string dropoff_datetime;
    int32_t passenger_count;
    double  trip_distance;
    int32_t ratecode_id;
    std::string store_and_fwd_flag;
    int32_t pu_location_id;
    int32_t do_location_id;
    int32_t payment_type;
    double  fare_amount;
    double  extra;
    double  mta_tax;
    double  tip_amount;
    double  tolls_amount;
    double  improvement_surcharge;
    double  total_amount;
    bool    valid;
};
```

**Why this design:**

- All 17 CSV columns are represented, each mapped to its natural C++ primitive. Integer columns (`vendor_id`, `passenger_count`, etc.) use `int32_t`. Monetary and distance columns use `double`. Datetime columns that the dataset stores as strings (`"2017-01-09 11:32:00"`) are kept as `std::string` — they are not needed for numeric range queries, so converting them to epoch seconds would add complexity with no payoff in Phase 1.
- The `valid` flag is set to `false` if any required numeric field fails to parse. Rows with `valid = false` are loaded but skipped during queries. This is intentional — it preserves the exact row count to match the dataset, which is important for memory analysis.
- **Array of Structs (AoS) layout:** The entire dataset is stored as `std::vector<TaxiTrip>`. Each struct occupies a contiguous block of memory and all its fields sit together. This is the natural C++ layout and is easy to reason about. Phase 3 will switch to a Struct of Arrays (SoA/columnar) layout and you will be able to directly compare the memory and query performance differences.

---

### `include/mini1/interfaces.hpp`

Defines two supporting structs and three abstract base classes (interfaces).

#### `LoadSummary`
```cpp
struct LoadSummary {
    std::size_t total_rows;
    std::size_t valid_rows;
    std::size_t invalid_rows;
};
```
Returned after a load so the caller knows how many rows parsed successfully vs failed. This is populated during the read loop in `CsvReader::read()`.

#### `RangeQuery`
```cpp
struct RangeQuery {
    std::string column;  // e.g. "trip_distance"
    double      low;     // lower bound
    double      high;    // upper bound
    bool        inclusive; // true = [low, high], false = (low, high)
};
```
A value type that describes what to search for. Decoupling the query description from the execution lets you create queries in one place (the benchmark's `main`) and pass them anywhere.

#### `IDataReader`
```cpp
virtual bool read(const std::string& source_path,
                  std::vector<TaxiTrip>& rows_out,
                  LoadSummary& summary_out) = 0;
```
The contract for anything that reads raw data and produces rows. The output is passed by reference rather than returned by value so the caller can pre-allocate if needed. Phase 2's `ParallelCsvReader` will implement this same interface.

#### `IDataStore`
```cpp
virtual void                         load(std::vector<TaxiTrip>&& rows) = 0;
virtual std::size_t                  row_count() const = 0;
virtual const TaxiTrip&              row_at(std::size_t index) const = 0;
virtual const std::vector<TaxiTrip>& rows() const = 0;
```
The contract for anything that holds loaded rows and provides indexed access. `rows()` returns the whole vector which is needed by `range_scan` in the query engine.

#### `IQueryEngine`
```cpp
virtual std::vector<std::size_t> range_search(
    const IDataStore& store,
    const RangeQuery& query) const = 0;
```
The contract for executing queries. It takes the store by const reference (read-only access) and the query parameters, returns a vector of **row indices** — not copies of the rows. This keeps memory usage flat regardless of how many rows match.

---

### `include/mini1/csv_reader.hpp` + `src/csv_reader.cpp`

`CsvReader` is the concrete implementation of `IDataReader`. It opens the file, reads it line by line, and parses each line into a `TaxiTrip`.

#### `CsvReader::trim(text)`
Strips leading and trailing whitespace from a string. Used everywhere a field value from the CSV might have spaces or carriage returns around it. Walks two pointers inward from both ends, returns `substr` — no heap allocation beyond what `substr` does.

#### `CsvReader::parse_csv_line(line)`
Splits one CSV line into a `vector<string>` of fields. It is RFC-4180 aware, meaning:
- Fields enclosed in double-quotes are handled correctly: `"hello, world"` produces one field `hello, world`.
- Escaped double-quotes inside quoted fields (`""`) are unescaped to a single `"`.
- Commas outside quotes are treated as delimiters.

Without this, loading real-world CSVs correctly is not possible — database exports routinely quote fields that contain commas.

#### `CsvReader::try_parse_int32(text, value)` and `try_parse_double(text, value)`
Type-safe string-to-number converters that return a bool indicating success. `try_parse_int32` uses `std::from_chars` from `<charconv>` which is the fastest standard way to convert an integer — it avoids locale dependency and does not throw. `try_parse_double` uses `std::strtod` (C-style) because `std::from_chars` for floating-point was not reliably implemented in all compilers at the time this was written, and `strtod` is locale-independent and fast. After conversion, `std::isfinite` checks that the result is not infinity or NaN.

#### `CsvReader::parse_row(fields)`
Maps the 17 parsed fields to the 17 members of a `TaxiTrip`. The mapping is positional — field index 0 is `vendor_id`, field index 16 is `total_amount`. If any mandatory numeric field fails to parse, the accumulator `ok` becomes false and `row.valid` is set to false. String fields (datetime, store_and_fwd_flag) never cause a row to become invalid — they are always accepted as-is.

#### `CsvReader::read(source_path, rows_out, summary_out)`
The main entry point. Opens the file with `std::ifstream`, skips the header line, then loops with `std::getline`. For each line it calls `parse_csv_line` then `parse_row`, appends the result to `rows_out`, and increments either `valid_rows` or `invalid_rows`. Returns `false` on file-open failure. The entire ~113 million rows are appended in a single pass — this is where the bulk of the wall time and memory allocation happens.

---

### `include/mini1/row_store.hpp` + `src/row_store.cpp`

`RowStore` is the concrete implementation of `IDataStore`. It holds the loaded rows in a single contiguous `std::vector<TaxiTrip>`.

#### `RowStore::load(rows)`
Takes the vector by rvalue reference (`&&`) and moves it into the internal `rows_` member. The `std::move` transfers ownership of the heap-allocated array without copying it — important because at 113 million rows, copying would double peak memory usage and take significant time.

#### `RowStore::row_count()`
Returns `rows_.size()` — the total number of rows including invalid ones.

#### `RowStore::row_at(index)`
Bounds-checked index access. Throws `std::out_of_range` if the index is invalid. The query engine calls this for every row during a scan, so it is the inner-loop hotspot.

#### `RowStore::rows()`
Returns a `const` reference to the entire vector. Used by `range_scan` to obtain the count and iterate.

---

### `include/mini1/query_engine.hpp` + `src/query_engine.cpp`

#### The `range_scan<Accessor>` template (in the header)

```cpp
template <typename Accessor>
std::vector<std::size_t> range_scan(const IDataStore& store,
                                    Accessor accessor,
                                    double low, double high,
                                    bool inclusive)
```

This is the core search function. It is a function template parameterized on `Accessor`, which is any callable that takes a `const TaxiTrip&` and returns a numeric value. The compiler instantiates a separate copy of this function for each distinct accessor type passed to it (each lambda is a unique type), producing specialized code for each column — no virtual dispatch or function pointer overhead in the inner loop.

What it does:
1. Pre-allocates the result vector with a heuristic `reserve(n / 100)` — assumes roughly 1% selectivity, avoiding early reallocation.
2. Iterates every row index from 0 to `n-1`.
3. Skips rows with `valid == false`.
4. Converts the field value to `double` (handles `int32_t` columns uniformly).
5. Applies the range check: inclusive `[low, high]` or exclusive `(low, high)`.
6. Appends matching indices to `result`.

Why return indices rather than rows: the caller gets exactly what it needs to do further work (e.g. fetch specific fields of matching rows) without a 113-million-row copy.

#### `QueryEngine::range_search(store, query)`

Dispatches the column name string from the `RangeQuery` to the appropriate field accessor lambda, then calls `range_scan`. Each `if` branch captures the right member pointer in a lambda and returns immediately. The column string is checked against both the original CSV header names (e.g. `"VendorID"`) and the normalized snake_case names (e.g. `"vendor_id"`) for convenience.

If the column name is not recognized, it returns an empty vector.

---

### `include/mini1/data_facade.hpp` + `src/data_facade.cpp`

`DataFacade` implements the **Facade pattern** — it presents a single, simple object to the outside world and hides the fact that three separate objects are working together.

#### Constructor (default)
```cpp
DataFacade::DataFacade()
    : reader_(std::make_unique<CsvReader>()),
      store_(std::make_unique<RowStore>()),
      engine_(std::make_unique<QueryEngine>()) {}
```
Creates concrete implementations via `std::make_unique`. Ownership is held as `std::unique_ptr` fields, so the reader, store, and engine are destroyed automatically when the facade is destroyed. No manual `delete` anywhere.

#### Constructor (dependency injection)
```cpp
DataFacade::DataFacade(unique_ptr<IDataReader>, unique_ptr<IDataStore>, unique_ptr<IQueryEngine>)
```
Accepts any objects that implement the interfaces. This is how Phase 2 and Phase 3 plug in parallel or columnar implementations — the benchmark simply passes different objects at construction time and the rest of the code stays identical.

#### `DataFacade::load(csv_path)`
1. Resets the stored `summary_`.
2. Creates a local `vector<TaxiTrip>`.
3. Calls `reader_->read(...)` — fills the local vector.
4. Calls `store_->load(std::move(...))` — transfers the vector into the store.
5. Returns `false` if the reader failed.

The local vector is moved into the store; no copy happens.

#### `DataFacade::range_search(query)`
Forwards directly to `engine_->range_search(*store_, query)`. The facade forwards the store by dereferencing its `unique_ptr` — the engine receives a `const IDataStore&`.

#### `DataFacade::row_at(index)` and `DataFacade::row_count()`
Simple pass-throughs to the store.

#### `DataFacade::load_summary()`
Returns the `LoadSummary` struct that was populated during `load()`.

---

### `apps/benchmark_phase1.cpp`

The standalone benchmark executable. It does not contain any reusable library logic — all library code lives in `src/`. This file only drives the library and collects metrics.

#### `UsageSnapshot`
```cpp
struct UsageSnapshot {
    double   user_ms;
    double   sys_ms;
    uint64_t rss;
    uint64_t footprint;
};
```
Captures the current resource usage state at a point in time. Two snapshots (before and after a phase) are differenced to get metrics for that phase.

- `user_ms` / `sys_ms`: CPU time spent in user space and kernel space (from `getrusage`). User time is time executing your own code. Sys time is time executing OS syscalls on your behalf (e.g. reading from disk into page cache).
- `rss`: Resident Set Size — how much RAM the process is physically using right now. From `getrusage` (`ru_maxrss`). On macOS this value is already in bytes; on Linux it is in kilobytes, so the code multiplies by 1024.
- `footprint`: macOS-specific `task_vm_info.phys_footprint` via the Mach kernel API. This is the number shown in Activity Monitor under "Memory". It accounts for compressed memory and is more accurate than RSS for understanding the process's real memory pressure on the system.

#### `snap()`
Calls `getrusage(RUSAGE_SELF, ...)` and (on macOS) `task_info(mach_task_self(), TASK_VM_INFO, ...)` to fill a `UsageSnapshot`. Called immediately before and after each measured phase.

#### `tv_to_ms(tv)`
Converts a POSIX `timeval` struct (seconds + microseconds) to a single millisecond value. Used to convert the `ru_utime` and `ru_stime` fields from `getrusage`.

#### `PhaseMetrics`
```cpp
struct PhaseMetrics {
    double   elapsed_ms;   // wall-clock time for this phase
    double   cpu_user;     // user-space CPU time consumed
    double   cpu_sys;      // kernel CPU time consumed
    double   cpu_total;    // cpu_user + cpu_sys
    uint64_t peak_rss;     // RSS at the end of this phase
    uint64_t footprint;    // phys_footprint at the end of this phase
};
```
The final metrics for one phase (load or query), computed by differencing two snapshots.

#### `measure(a, b, elapsed)`
Builds a `PhaseMetrics` from two `UsageSnapshot` values and the independently measured wall-clock duration. CPU times are differenced (end minus start). RSS and footprint use the end snapshot value — they are max values that only grow.

#### `print_report(...)` and `csv_line(...)`
Two output formatters for the same data. `print_report` writes a labeled human-readable report to stdout for quick manual inspection. `csv_line` writes a single comma-separated row of 18 numeric values for machine consumption. The benchmark script calls with `--csv` and collects these rows into `phase1_results.csv`.

#### `main()`
Parses command-line arguments, runs the load phase (surrounded by `snap()` + `steady_clock` calls), then runs the query phase with `reps` repetitions (also surrounded by measurement calls). The `reps` loop runs the same query multiple times so you get a stable average — a single query over 113M rows has some run-to-run variance from CPU cache state, OS scheduling, etc. Averaging 10 runs gives a much more reliable mean.

---

### `scripts/run_phase1_benchmark.sh`

Runs `benchmark_phase1` N times (default 10) and collects a `phase1_results.csv`. On each iteration it prepends a UTC timestamp and an incrementing run number to the benchmark's `--csv` output. The 25-column CSV header it writes is:

```
timestamp_utc, run, dataset_path, query_column, low, high, query_repeats,
load_ms, query_total_ms, avg_query_ms, last_hits,
total_rows, valid_rows, invalid_rows,
load_cpu_user_ms, load_cpu_sys_ms, load_cpu_total_ms,
query_cpu_user_ms, query_cpu_sys_ms, query_cpu_total_ms,
load_peak_rss_bytes, load_footprint_bytes,
query_peak_rss_bytes, query_footprint_bytes, overall_peak_rss_bytes
```

Usage:
```bash
./scripts/run_phase1_benchmark.sh <csv_path> [runs] [column] [low] [high]
```

### `scripts/run_phase1_pipeline.sh`

One-command end-to-end: runs the benchmark collection script, then creates a Python virtual environment at `phase-1/.venv` if it does not exist, installs plotting dependencies, and runs the plot script. Saves you from running three separate commands between a code change and seeing charts.

Usage:
```bash
./scripts/run_phase1_pipeline.sh <csv_path> [runs] [column] [low] [high] [outdir]
```

### `scripts/plot_phase1_benchmark.py`

Reads `phase1_results.csv` and writes an HTML report plus four SVG charts to an output directory.

- **`load_data(csv_path)`** — reads the CSV with pandas, coerces all numeric columns, parses the UTC timestamp column.
- **`write_summary(df, outdir)`** — computes per-metric statistics (count, mean, stddev, min, p50, p95, max) and writes `summary_stats.csv`.
- **`plot_time_trends`** — line chart of `load_ms` and `avg_query_ms` across runs, so you can see if performance is stable or drifting between runs.
- **`plot_cpu_trends`** — line chart of load and query CPU total ms.
- **`plot_memory_trend`** — line chart of Peak RSS, Load Footprint, and Query Footprint in GB across runs.
- **`plot_distributions`** — box-and-whisker chart showing the distribution of the four key timing metrics across all runs.
- **`write_html_report`** — embeds the four SVGs and a KPI table into a self-contained `report.html`.

The `resolve_input_path` and `resolve_outdir_path` helpers look for the input CSV in multiple locations (CWD, script directory, parent directory) so the script works correctly whether you run it from `phase-1/` or from `phase-1/scripts/`.

---

## Data Flow

A complete trace of what happens when you call `data.load(path)` followed by `data.range_search(query)`:

```
1.  DataFacade::load(path)
      |
      +-> reader_->read(path, rows, summary)      [CsvReader::read]
            |
            +-> std::ifstream::getline(line)       [for each of ~113M lines]
            +-> parse_csv_line(line)               [split on commas, handle quotes]
            +-> parse_row(fields)                  [map fields[0..16] to TaxiTrip members]
            +-> rows.push_back(TaxiTrip)           [accumulate into local vector]
            |
            returns true
      |
      +-> store_->load(std::move(rows))            [RowStore::load]
            |
            +-> rows_ = std::move(rows)            [O(1) move, no copy]

2.  DataFacade::range_search(query)
      |
      +-> engine_->range_search(*store_, query)    [QueryEngine::range_search]
            |
            +-> dispatch column name to accessor lambda
            +-> range_scan<Accessor>(store, accessor, lo, hi, inclusive)
                  |
                  +-> for i in [0, row_count):
                        row = store.row_at(i)
                        if !row.valid: skip
                        val = accessor(row)        [e.g. row.trip_distance]
                        if val in [lo, hi]: result.push_back(i)
                  |
                  returns vector<size_t>  (matching row indices)
```

---

## Build

```bash
cd phase-1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary lands at `phase-1/build/benchmark_phase1`.

---

## Running the Benchmark

```bash
# Default query: trip_distance in [1.0, 3.0], 10 repeats
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv

# Custom: fare_amount in [10, 50], 10 repeats
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv fare_amount 10 50 10

# Machine-readable CSV output (used by the collection scripts)
./build/benchmark_phase1 ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv trip_distance 1 3 10 --csv
```

## Collecting Multiple Runs

```bash
# 10 runs, default query
./scripts/run_phase1_benchmark.sh ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv 10

# Full pipeline: collect + plot + HTML report
./scripts/run_phase1_pipeline.sh ~/Downloads/2017_Yellow_Taxi_Trip_Data_20260228.csv 10 trip_distance 1 3 phase1_report
```

## Generate Plots Separately

```bash
pip install -r scripts/requirements.txt
python3 scripts/plot_phase1_benchmark.py --input phase1_results.csv --outdir phase1_report
```

Outputs:
- `phase1_report/summary_stats.csv`
- `phase1_report/time_trends.svg`
- `phase1_report/cpu_trends.svg`
- `phase1_report/memory_trend.svg`
- `phase1_report/distributions.svg`
- `phase1_report/report.html`

---

## Understanding the Metrics

| Metric | What it measures | Why it matters |
|---|---|---|
| `load_ms` | Wall-clock time to read the entire CSV and load into memory | The biggest cost in Phase 1. I/O + parsing time. |
| `avg_query_ms` | Average wall-clock time per single range scan | How fast you can query 113M rows serially. |
| `load_cpu_user_ms` | CPU time in user space during load | Reflects parsing cost (string splitting, number conversion). |
| `load_cpu_sys_ms` | CPU time in the kernel during load | Reflects disk I/O — reading pages from disk into page cache. |
| `load_footprint_bytes` | Physical memory used after load (macOS `phys_footprint`) | The actual RAM the OS allocates for all row data. Matches Activity Monitor. |
| `query_cpu_total_ms` | CPU time (user + sys) during all query repetitions | Reflects how much compute the serial scan costs. |
| `overall_peak_rss` | Max RSS at end of benchmark | Coarser than footprint but portable to Linux. |

On this dataset (~113M rows × ~200 bytes per `TaxiTrip` struct) expect peak memory around 20–25 GB — exceeding the 16 GB physical RAM and forcing swap use, which dramatically increases load time.

---

## Queryable Columns

Any of these column names can be passed as the `column` argument:

| CSV name | snake_case alias | Type |
|---|---|---|
| `VendorID` | `vendor_id` | int |
| — | `passenger_count` | int |
| — | `trip_distance` | double |
| `RatecodeID` | `ratecode_id` | int |
| `PULocationID` | `pu_location_id` | int |
| `DOLocationID` | `do_location_id` | int |
| — | `payment_type` | int |
| — | `fare_amount` | double |
| — | `extra` | double |
| — | `mta_tax` | double |
| — | `tip_amount` | double |
| — | `tolls_amount` | double |
| — | `improvement_surcharge` | double |
| — | `total_amount` | double |
