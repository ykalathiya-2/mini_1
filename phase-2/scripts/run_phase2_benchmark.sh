#!/usr/bin/env bash
# run_phase2_benchmark.sh
# Collect N benchmark runs into a CSV file.
#
# Usage:
#   ./run_phase2_benchmark.sh <csv_path> [runs] [column] [low] [high] [threads] [reps]
#
# Output: phase2_results.csv in the project root

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/build/benchmark_phase2"

CSV_PATH="${1:?Usage: $0 <csv_path> [runs] [column] [low] [high] [threads] [reps]}"
RUNS="${2:-5}"
COLUMN="${3:-trip_distance}"
LOW="${4:-1}"
HIGH="${5:-3}"
THREADS="${6:-0}"
REPS="${7:-10}"
OUTPUT="$PROJECT_ROOT/phase2_results.csv"

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: $BINARY not found. Build with:" >&2
    echo "  cd $PROJECT_ROOT && cmake -S . -B build \\" >&2
    echo "    -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm@20/bin/clang \\" >&2
    echo "    -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@20/bin/clang++ \\" >&2
    echo "    -DCMAKE_BUILD_TYPE=Release && cmake --build build -j" >&2
    exit 1
fi

echo "timestamp_utc,run,dataset,column,low,high,reps,threads,\
total_rows,valid_rows,invalid_rows,\
load_serial_ms,load_parallel_ms,load_speedup,\
query_serial_ms,query_parallel_ms,query_speedup,\
hits_serial,hits_parallel,\
load_s_cpu_user_ms,load_s_cpu_sys_ms,load_s_footprint,\
load_p_cpu_user_ms,load_p_cpu_sys_ms,load_p_footprint,\
query_s_cpu_total_ms,query_p_cpu_total_ms" > "$OUTPUT"

for i in $(seq 1 "$RUNS"); do
    echo "  run $i / $RUNS ..."
    TS=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
    LINE=$("$BINARY" "$CSV_PATH" "$COLUMN" "$LOW" "$HIGH" "$REPS" "$THREADS" --csv)
    echo "${TS},${i},${LINE}" >> "$OUTPUT"
done

echo "Wrote $RUNS rows to $OUTPUT"
