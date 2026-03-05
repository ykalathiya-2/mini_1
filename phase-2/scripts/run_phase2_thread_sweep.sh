#!/usr/bin/env bash
# run_phase2_thread_sweep.sh
# Run benchmark at thread counts 1, 2, 4, 8, all-cores and collect results.
# This produces the data needed for speedup and scaling plots.
#
# Usage:
#   ./run_phase2_thread_sweep.sh <csv_path> [runs_per_thread] [column] [low] [high] [reps]
#
# Output: phase2_thread_sweep.csv in the project root

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/build/benchmark_phase2"

CSV_PATH="${1:?Usage: $0 <csv_path> [runs_per_thread] [column] [low] [high] [reps]}"
RUNS="${2:-3}"
COLUMN="${3:-trip_distance}"
LOW="${4:-1}"
HIGH="${5:-3}"
REPS="${6:-5}"
OUTPUT="$PROJECT_ROOT/phase2_thread_sweep.csv"

NCORES=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 1)

# Thread counts to sweep: powers of 2 up to NCORES, then NCORES itself.
THREAD_COUNTS=(1 2 4 8)
if [[ $NCORES -gt 8 ]]; then THREAD_COUNTS+=("$NCORES"); fi
if [[ $NCORES -le 8 && ! " ${THREAD_COUNTS[*]} " =~ " $NCORES " ]]; then
    THREAD_COUNTS+=("$NCORES")
fi

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

for T in "${THREAD_COUNTS[@]}"; do
    echo "--- threads=$T ---"
    for i in $(seq 1 "$RUNS"); do
        echo "  run $i / $RUNS (threads=$T) ..."
        TS=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
        LINE=$("$BINARY" "$CSV_PATH" "$COLUMN" "$LOW" "$HIGH" "$REPS" "$T" --csv)
        echo "${TS},${i},${LINE}" >> "$OUTPUT"
    done
done

echo "Thread sweep complete: $OUTPUT"
echo "Thread counts swept: ${THREAD_COUNTS[*]}"
