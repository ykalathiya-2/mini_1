#!/usr/bin/env bash
# =============================================================================
# benchmark.sh — Unified benchmark runner for mini_1 phase-1, phase-2, phase-3
#
# Usage:
#   ./scripts/benchmark.sh [OPTIONS] <csv_path>
#
# Options:
#   --phases 1,2,3        Comma-separated list of phases to run (default: 1,2,3)
#   --column <name>       Query column          (default: trip_distance)
#   --low    <val>        Range low bound       (default: 1)
#   --high   <val>        Range high bound      (default: 3)
#   --runs   <n>          Outer benchmark runs  (default: 5)
#   --reps   <n>          Inner query repeats   (default: 10)
#   --threads <n>         OMP threads, 0=auto   (default: 0)
#   --out    <dir>        Output directory      (default: ./benchmark_results)
#   --no-build            Skip cmake build step
#   -h, --help            Show this help
#
# Examples:
#   # All phases, default query
#   ./scripts/benchmark.sh /tmp/taxi_5m.csv
#
#   # Only phase-1 and phase-3
#   ./scripts/benchmark.sh --phases 1,3 /tmp/taxi_5m.csv
#
#   # Phase-2 only, custom column and thread count
#   ./scripts/benchmark.sh --phases 2 --column fare_amount --low 5 --high 20 --threads 4 /tmp/taxi_5m.csv
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---- defaults ----------------------------------------------------------------
PHASES="1,2,3"
COLUMN="trip_distance"
LOW="1"
HIGH="3"
RUNS=5
REPS=10
THREADS=0
OUT_DIR="$REPO_ROOT/benchmark_results"
DO_BUILD=true
CSV_PATH=""

# ---- argument parsing --------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --phases)   PHASES="$2";  shift 2 ;;
        --column)   COLUMN="$2";  shift 2 ;;
        --low)      LOW="$2";     shift 2 ;;
        --high)     HIGH="$2";    shift 2 ;;
        --runs)     RUNS="$2";    shift 2 ;;
        --reps)     REPS="$2";    shift 2 ;;
        --threads)  THREADS="$2"; shift 2 ;;
        --out)      OUT_DIR="$2"; shift 2 ;;
        --no-build) DO_BUILD=false; shift ;;
        -h|--help)
            sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*)
            echo "Unknown option: $1" >&2; exit 1 ;;
        *)
            CSV_PATH="$1"; shift ;;
    esac
done

if [[ -z "$CSV_PATH" ]]; then
    echo "ERROR: csv_path is required." >&2
    echo "Run '$0 --help' for usage." >&2
    exit 1
fi

if [[ ! -f "$CSV_PATH" ]]; then
    echo "ERROR: CSV file not found: $CSV_PATH" >&2
    exit 1
fi

# Convert PHASES string to array
IFS=',' read -ra PHASE_LIST <<< "$PHASES"

# Validate
for P in "${PHASE_LIST[@]}"; do
    if [[ "$P" != "1" && "$P" != "2" && "$P" != "3" ]]; then
        echo "ERROR: Invalid phase '$P'. Must be 1, 2, or 3." >&2
        exit 1
    fi
done

mkdir -p "$OUT_DIR"
TIMESTAMP="$(date -u +"%Y%m%dT%H%M%SZ")"

# ---- compiler detection ------------------------------------------------------
detect_cxx() {
    for candidate in \
        /opt/homebrew/opt/llvm@20/bin/clang++ \
        /opt/homebrew/opt/llvm/bin/clang++ \
        clang++ g++ c++; do
        if command -v "$candidate" &>/dev/null; then
            echo "$candidate"; return
        fi
    done
    echo "c++"
}
detect_cc() {
    for candidate in \
        /opt/homebrew/opt/llvm@20/bin/clang \
        /opt/homebrew/opt/llvm/bin/clang \
        clang gcc cc; do
        if command -v "$candidate" &>/dev/null; then
            echo "$candidate"; return
        fi
    done
    echo "cc"
}
CXX_COMPILER="$(detect_cxx)"
C_COMPILER="$(detect_cc)"

# ---- build function ----------------------------------------------------------
build_phase() {
    local phase="$1"
    local phase_dir="$REPO_ROOT/phase-$phase"
    local build_dir="$phase_dir/build"
    echo "[build] phase-$phase using $CXX_COMPILER ..."
    cmake -S "$phase_dir" -B "$build_dir" \
        -DCMAKE_C_COMPILER="$C_COMPILER" \
        -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
        --log-level=WARNING \
        -Wno-dev \
        > /dev/null
    cmake --build "$build_dir" -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)" \
        > /dev/null
    echo "[build] phase-$phase done."
}

# ---- CSV header writers ------------------------------------------------------
write_header_p1() {
    echo "timestamp_utc,run,phase,dataset,column,low,high,reps,\
load_ms,query_total_ms,avg_query_ms,hits,\
total_rows,valid_rows,invalid_rows,\
load_cpu_user_ms,load_cpu_sys_ms,load_cpu_total_ms,\
query_cpu_user_ms,query_cpu_sys_ms,query_cpu_total_ms,\
load_peak_rss_bytes,load_footprint_bytes,\
query_peak_rss_bytes,query_footprint_bytes,overall_peak_rss_bytes"
}

write_header_p2() {
    echo "timestamp_utc,run,phase,dataset,predicates,reps,threads,\
total_rows,valid_rows,invalid_rows,\
load_parallel_ms,query_parallel_ms,hits,\
load_cpu_user_ms,load_cpu_sys_ms,load_footprint,\
query_cpu_total_ms"
}

write_header_p3() {
    echo "timestamp_utc,run,phase,dataset,column,low,high,reps,threads,\
total_rows,valid_rows,invalid_rows,\
load_ms,load_footprint_bytes,\
query_ms,hits"
}

# ---- Phase-3 binary produces human-readable output only; we parse it --------
run_p3_once() {
    local binary="$1" csv="$2" col="$3" lo="$4" hi="$5" reps="$6" thr="$7"
    local out
    out="$("$binary" "$csv" "$col" "$lo" "$hi" "$reps" "$thr")"

    local total valid invalid load_ms fp qms hits
    total=$(echo "$out"   | sed -n 's/.*total rows:[[:space:]]*\([0-9]*\).*/\1/p')
    valid=$(echo "$out"   | sed -n '/invalid/d; s/.*valid rows:[[:space:]]*\([0-9]*\).*/\1/p')
    invalid=$(echo "$out" | sed -n 's/.*invalid rows:[[:space:]]*\([0-9]*\).*/\1/p')
    load_ms=$(echo "$out" | sed -n 's/.*SoA parallel load wall ms:[[:space:]]*\([0-9.]*\).*/\1/p')
    fp=$(echo "$out"      | sed -n 's/.*footprint GB:[[:space:]]*\([0-9.]*\).*/\1/p')
    qms=$(echo "$out"     | sed -n 's/.*-thread SoA wall ms:[[:space:]]*\([0-9.]*\).*/\1/p')
    hits=$(echo "$out"    | sed -n 's/.*hits=\([0-9]*\).*/\1/p')

    # Convert footprint GB → bytes (approximate)
    local fp_bytes
    fp_bytes=$(awk "BEGIN{printf \"%.0f\", ${fp:-0} * 1e9}")

    echo "${total},${valid},${invalid},${load_ms},${fp_bytes},${qms},${hits}"
}

# ---- run phases --------------------------------------------------------------
echo "=============================================="
echo " mini_1 unified benchmark"
echo " phases  : $PHASES"
echo " csv     : $CSV_PATH"
echo " query   : $COLUMN in [$LOW, $HIGH]"
echo " runs    : $RUNS  reps: $REPS  threads: $THREADS"
echo " output  : $OUT_DIR"
echo "=============================================="
echo ""

for P in "${PHASE_LIST[@]}"; do
    BINARY="$REPO_ROOT/phase-$P/build/benchmark_phase$P"
    OUTFILE="$OUT_DIR/phase${P}_${TIMESTAMP}.csv"

    # Build if needed
    if [[ "$DO_BUILD" == true ]]; then
        build_phase "$P"
    fi

    if [[ ! -x "$BINARY" ]]; then
        echo "ERROR: binary not found: $BINARY" >&2
        echo "Build first or re-run without --no-build." >&2
        continue
    fi

    echo "--- Phase $P ---"

    case "$P" in
    1)
        write_header_p1 > "$OUTFILE"
        for ((i=1; i<=RUNS; i++)); do
            TS="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
            LINE="$("$BINARY" "$CSV_PATH" "$COLUMN" "$LOW" "$HIGH" "$REPS" --csv)"
            echo "${TS},${i},phase-1,${CSV_PATH},${COLUMN},${LOW},${HIGH},${REPS},${LINE}" >> "$OUTFILE"
            echo "  run $i/$RUNS"
        done
        ;;
    2)
        write_header_p2 > "$OUTFILE"
        for ((i=1; i<=RUNS; i++)); do
            TS="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
            LINE="$("$BINARY" "$CSV_PATH" "${COLUMN}:${LOW}:${HIGH}" "$REPS" "$THREADS" --csv)"
            echo "${TS},${i},phase-2,${LINE}" >> "$OUTFILE"
            echo "  run $i/$RUNS"
        done
        ;;
    3)
        write_header_p3 > "$OUTFILE"
        for ((i=1; i<=RUNS; i++)); do
            TS="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
            LINE="$(run_p3_once "$BINARY" "$CSV_PATH" "$COLUMN" "$LOW" "$HIGH" "$REPS" "$THREADS")"
            echo "${TS},${i},phase-3,${CSV_PATH},${COLUMN},${LOW},${HIGH},${REPS},${THREADS},${LINE}" >> "$OUTFILE"
            echo "  run $i/$RUNS"
        done
        ;;
    esac

    echo "  Wrote $OUTFILE"
    echo ""
done

# ---- summary table -----------------------------------------------------------
echo "=============================================="
echo " Summary"
echo "=============================================="
for P in "${PHASE_LIST[@]}"; do
    OUTFILE="$OUT_DIR/phase${P}_${TIMESTAMP}.csv"
    if [[ -f "$OUTFILE" ]]; then
        echo ""
        echo "Phase $P results: $OUTFILE"
        case "$P" in
        1)
            awk -F',' 'NR>1{s+=$9; q+=$11; n++} END{
                printf "  avg load_ms     : %.1f\n  avg avg_query_ms: %.3f\n", (n>0?s/n:0), (n>0?q/n:0)
            }' "$OUTFILE"
            ;;
        2)
            # Predicates field (col 5) is quoted and may contain commas.
            # Remove the quoted field first, then parse remaining columns.
            sed 's/"[^"]*"/PRED/' "$OUTFILE" | awk -F',' 'NR>1{l+=$11; q+=$12; n++} END{
                printf "  avg load_ms  : %.1f\n  avg query_ms : %.3f\n", (n>0?l/n:0), (n>0?q/n:0)
            }'
            ;;
        3)
            awk -F',' 'NR>1{l+=$13; q+=$15; n++} END{
                printf "  avg load_ms  : %.1f\n  avg query_ms : %.3f\n", (n>0?l/n:0), (n>0?q/n:0)
            }' "$OUTFILE"
            ;;
        esac
    fi
done
echo ""
echo "Done."
