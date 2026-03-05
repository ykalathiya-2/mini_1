#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <csv_path> [runs] [column] [low] [high]"
  exit 1
fi

CSV_PATH="$1"
RUNS="${2:-10}"
COLUMN="${3:-trip_distance}"
LOW="${4:-1}"
HIGH="${5:-3}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

OUT="$PROJECT_ROOT/phase1_results.csv"
echo "timestamp_utc,run,dataset_path,query_column,low,high,query_repeats,load_ms,query_total_ms,avg_query_ms,last_hits,total_rows,valid_rows,invalid_rows,load_cpu_user_ms,load_cpu_sys_ms,load_cpu_total_ms,query_cpu_user_ms,query_cpu_sys_ms,query_cpu_total_ms,load_peak_rss_bytes,load_footprint_bytes,query_peak_rss_bytes,query_footprint_bytes,overall_peak_rss_bytes" > "$OUT"

for ((i=1; i<=RUNS; i++)); do
  timestamp_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  metrics_csv="$("$PROJECT_ROOT/build/benchmark_phase1" "$CSV_PATH" "$COLUMN" "$LOW" "$HIGH" 10 --csv)"

  echo "$timestamp_utc,$i,$CSV_PATH,$COLUMN,$LOW,$HIGH,10,$metrics_csv" >> "$OUT"
  echo "Completed run $i/$RUNS"
done

echo "Wrote $OUT"
