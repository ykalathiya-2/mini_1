#!/usr/bin/env python3
"""
plot_phase2_results.py
Plot Phase 2 benchmark results: speedup curves, wall time, CPU utilisation.

Usage:
    python3 plot_phase2_results.py --input phase2_thread_sweep.csv --outdir report
    python3 plot_phase2_results.py --input phase2_results.csv       --outdir report
"""

import argparse
import sys
from pathlib import Path

import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import seaborn as sns


def load_data(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    numeric_cols = [c for c in df.columns if c not in ("timestamp_utc", "dataset", "column")]
    df[numeric_cols] = df[numeric_cols].apply(pd.to_numeric, errors="coerce")
    if "timestamp_utc" in df.columns:
        df["timestamp_utc"] = pd.to_datetime(df["timestamp_utc"], utc=True, errors="coerce")
    return df


def write_summary(df: pd.DataFrame, outdir: Path) -> None:
    key_cols = [
        "threads", "load_serial_ms", "load_parallel_ms", "load_speedup",
        "query_serial_ms", "query_parallel_ms", "query_speedup",
    ]
    available = [c for c in key_cols if c in df.columns]
    summary = df[available].groupby("threads").agg(
        ["count", "mean", "std", "min",
         lambda x: x.quantile(0.50),
         lambda x: x.quantile(0.95),
         "max"]
    )
    summary.columns = ["_".join(c).strip() for c in summary.columns]
    summary.to_csv(outdir / "summary_stats.csv")


STYLE = {"style": "whitegrid", "palette": "muted"}


def plot_speedup_vs_threads(df: pd.DataFrame, outdir: Path) -> None:
    """Load speedup and query speedup vs thread count."""
    if "threads" not in df.columns:
        return

    sns.set_theme(**STYLE)
    fig, ax = plt.subplots(figsize=(8, 5))

    for col, label, marker in [
        ("load_speedup",  "Load speedup",  "o"),
        ("query_speedup", "Query speedup", "s"),
    ]:
        if col not in df.columns:
            continue
        means = df.groupby("threads")[col].mean().reset_index()
        ax.plot(means["threads"], means[col], marker=marker, label=label, linewidth=2)

    # Ideal linear reference line
    threads = sorted(df["threads"].unique())
    ax.plot(threads, threads, "--", color="gray", linewidth=1, label="ideal linear")

    ax.set_xlabel("Threads")
    ax.set_ylabel("Speedup (relative to serial)")
    ax.set_title("Speedup vs Thread Count")
    ax.legend()
    ax.set_xticks(threads)
    plt.tight_layout()
    fig.savefig(outdir / "speedup_vs_threads.svg")
    plt.close(fig)


def plot_wall_time_vs_threads(df: pd.DataFrame, outdir: Path) -> None:
    """Parallel wall time for load and query vs thread count."""
    if "threads" not in df.columns:
        return

    sns.set_theme(**STYLE)
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    threads = sorted(df["threads"].unique())

    for ax, (serial_col, parallel_col, title) in zip(axes, [
        ("load_serial_ms",  "load_parallel_ms",  "Load Wall Time (ms)"),
        ("query_serial_ms", "query_parallel_ms", "Query Wall Time (ms)"),
    ]):
        for col, label, marker in [
            (serial_col,   "serial",   "o"),
            (parallel_col, "parallel", "s"),
        ]:
            if col not in df.columns:
                continue
            means = df.groupby("threads")[col].mean().reset_index()
            ax.plot(means["threads"], means[col], marker=marker, label=label, linewidth=2)
        ax.set_xlabel("Threads")
        ax.set_ylabel("Wall time (ms)")
        ax.set_title(title)
        ax.legend()
        ax.set_xticks(threads)

    plt.tight_layout()
    fig.savefig(outdir / "wall_time_vs_threads.svg")
    plt.close(fig)


def plot_cpu_utilisation(df: pd.DataFrame, outdir: Path) -> None:
    """Total CPU time vs thread count — shows how well cores are used."""
    if "threads" not in df.columns:
        return
    if "query_p_cpu_total_ms" not in df.columns and "query_s_cpu_total_ms" not in df.columns:
        return

    sns.set_theme(**STYLE)
    fig, ax = plt.subplots(figsize=(8, 5))
    threads = sorted(df["threads"].unique())

    for col, label, marker in [
        ("query_s_cpu_total_ms", "Query serial CPU",   "o"),
        ("query_p_cpu_total_ms", "Query parallel CPU", "s"),
        ("load_s_cpu_user_ms",   "Load serial CPU",    "D"),
        ("load_p_cpu_user_ms",   "Load parallel CPU",  "^"),
    ]:
        if col not in df.columns:
            continue
        means = df.groupby("threads")[col].mean().reset_index()
        ax.plot(means["threads"], means[col], marker=marker, label=label, linewidth=2)

    ax.set_xlabel("Threads")
    ax.set_ylabel("CPU time (ms)")
    ax.set_title("CPU Time vs Thread Count")
    ax.legend()
    ax.set_xticks(threads)
    plt.tight_layout()
    fig.savefig(outdir / "cpu_utilisation.svg")
    plt.close(fig)


def plot_distributions(df: pd.DataFrame, outdir: Path) -> None:
    """Box plots of key metrics across runs."""
    sns.set_theme(**STYLE)
    cols = {
        "load_serial_ms": "Load\nserial ms",
        "load_parallel_ms": "Load\nparallel ms",
        "query_serial_ms": "Query\nserial ms",
        "query_parallel_ms": "Query\nparallel ms",
    }
    available = {k: v for k, v in cols.items() if k in df.columns}
    if not available:
        return

    fig, ax = plt.subplots(figsize=(10, 5))
    plot_data = df[[*available.keys()]].copy()
    plot_data.columns = list(available.values())
    sns.boxplot(data=plot_data, ax=ax)
    ax.set_ylabel("Wall time (ms)")
    ax.set_title("Timing Distributions Across All Runs")
    plt.tight_layout()
    fig.savefig(outdir / "distributions.svg")
    plt.close(fig)


def write_html_report(df: pd.DataFrame, outdir: Path) -> None:
    def mean(col):
        return f"{df[col].mean():.1f}" if col in df.columns else "n/a"

    threads_list = sorted(df["threads"].unique()) if "threads" in df.columns else []

    rows_html = ""
    for t in threads_list:
        sub = df[df["threads"] == t]
        rows_html += f"""
        <tr>
            <td>{t}</td>
            <td>{sub['load_parallel_ms'].mean():.0f}</td>
            <td>{sub['load_speedup'].mean():.2f}x</td>
            <td>{sub['query_parallel_ms'].mean():.1f}</td>
            <td>{sub['query_speedup'].mean():.2f}x</td>
        </tr>""" if all(c in sub.columns for c in
                        ["load_parallel_ms","load_speedup","query_parallel_ms","query_speedup"]) else ""

    charts = ""
    for fname, title in [
        ("speedup_vs_threads.svg",  "Speedup vs Thread Count"),
        ("wall_time_vs_threads.svg","Wall Time vs Thread Count"),
        ("cpu_utilisation.svg",     "CPU Utilisation"),
        ("distributions.svg",       "Timing Distributions"),
    ]:
        if (outdir / fname).exists():
            charts += f'<h2>{title}</h2><img src="{fname}" style="max-width:100%">\n'

    html = f"""<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>Phase 2 Benchmark Report</title>
<style>
  body {{ font-family: monospace; max-width: 960px; margin: auto; padding: 2em; }}
  table {{ border-collapse: collapse; width: 100%; margin-bottom: 2em; }}
  th, td {{ border: 1px solid #ccc; padding: 6px 12px; text-align: right; }}
  th {{ background: #f0f0f0; }}
</style>
</head><body>
<h1>Phase 2 — OpenMP Parallel Benchmark</h1>
<p>Dataset: {df['dataset'].iloc[0] if 'dataset' in df.columns else 'unknown'}</p>
<p>Total runs: {len(df)}</p>

<h2>Results by Thread Count</h2>
<table>
  <tr><th>Threads</th><th>Load parallel (ms)</th><th>Load speedup</th>
      <th>Query parallel (ms)</th><th>Query speedup</th></tr>
  {rows_html}
</table>

{charts}
</body></html>
"""
    (outdir / "report.html").write_text(html)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input",  required=True, help="path to results CSV")
    parser.add_argument("--outdir", default="phase2_report", help="output directory")
    args = parser.parse_args()

    csv_path = Path(args.input)
    if not csv_path.exists():
        sys.exit(f"ERROR: {csv_path} not found")

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    df = load_data(csv_path)
    if df.empty:
        sys.exit("ERROR: no data in CSV")

    write_summary(df, outdir)
    plot_speedup_vs_threads(df, outdir)
    plot_wall_time_vs_threads(df, outdir)
    plot_cpu_utilisation(df, outdir)
    plot_distributions(df, outdir)
    write_html_report(df, outdir)

    print(f"Report written to {outdir}/report.html")


if __name__ == "__main__":
    main()
