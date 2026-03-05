#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <csv_path> [runs] [column] [low] [high] [outdir]"
  exit 1
fi

CSV_PATH="$1"
RUNS="${2:-10}"
COLUMN="${3:-trip_distance}"
LOW="${4:-1}"
HIGH="${5:-3}"
OUTDIR="${6:-phase1_report}"

BENCHMARK_BIN="$PROJECT_ROOT/build/benchmark_phase1"
RESULTS_CSV="$PROJECT_ROOT/phase1_results.csv"
REQUIREMENTS_FILE="$SCRIPT_DIR/requirements.txt"
VENV_DIR="$PROJECT_ROOT/.venv"
PYTHON_BIN="$VENV_DIR/bin/python"

if [[ ! -x "$BENCHMARK_BIN" ]]; then
  echo "Benchmark binary not found: $BENCHMARK_BIN"
  echo "Build first with: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
  exit 2
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found in PATH."
  exit 4
fi

ensure_project_venv() {
  if [[ -x "$PYTHON_BIN" ]]; then
    return 0
  fi
  echo "Creating project virtual environment at $VENV_DIR"
  python3 -m venv "$VENV_DIR"
}

ensure_plot_dependencies() {
  if "$PYTHON_BIN" - <<'PY' >/dev/null 2>&1
import pandas
import matplotlib
import seaborn
PY
  then
    return 0
  fi
  echo "Plot dependencies missing; installing from $REQUIREMENTS_FILE"
  "$PYTHON_BIN" -m pip install -r "$REQUIREMENTS_FILE"
}

echo "[1/2] Running benchmark collection..."
"$SCRIPT_DIR/run_phase1_benchmark.sh" "$CSV_PATH" "$RUNS" "$COLUMN" "$LOW" "$HIGH"

if [[ ! -f "$RESULTS_CSV" ]]; then
  echo "Expected results file not found: $RESULTS_CSV"
  exit 3
fi

echo "[2/2] Generating plots and HTML report..."
ensure_project_venv
ensure_plot_dependencies
(cd "$PROJECT_ROOT" && "$PYTHON_BIN" scripts/plot_phase1_benchmark.py --input "$RESULTS_CSV" --outdir "$OUTDIR")

echo "Done. Results: $RESULTS_CSV"
echo "Done. Report dir: $PROJECT_ROOT/$OUTDIR"
