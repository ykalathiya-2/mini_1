#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Phase 1 benchmark charts and HTML report.")
    parser.add_argument("--input", default="phase1_results.csv", help="Input benchmark CSV")
    parser.add_argument("--outdir", default="phase1_report", help="Output directory")
    return parser.parse_args()


def resolve_input_path(raw_input: str) -> Path:
    requested = Path(raw_input)
    if requested.exists():
        return requested.resolve()

    script_dir = Path(__file__).resolve().parent
    candidates = [
        Path.cwd() / raw_input,
        script_dir / raw_input,
        script_dir.parent / raw_input,
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    tried = "\n".join(f"- {p.resolve()}" for p in candidates)
    raise FileNotFoundError(
        f"Could not find input CSV '{raw_input}'. Tried:\n{tried}\n"
        f"Run benchmark first to create phase1_results.csv."
    )


def resolve_outdir_path(raw_outdir: str) -> Path:
    requested = Path(raw_outdir)
    if requested.is_absolute():
        return requested

    script_dir = Path(__file__).resolve().parent
    return (Path.cwd() / requested) if Path.cwd().name != "scripts" else (script_dir.parent / requested)


def load_data(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)

    numeric_cols = [
        "run", "low", "high", "query_repeats",
        "load_ms", "query_total_ms", "avg_query_ms", "last_hits",
        "total_rows", "valid_rows", "invalid_rows",
        "load_cpu_user_ms", "load_cpu_sys_ms", "load_cpu_total_ms",
        "query_cpu_user_ms", "query_cpu_sys_ms", "query_cpu_total_ms",
        "load_peak_rss_bytes", "load_footprint_bytes",
        "query_peak_rss_bytes", "query_footprint_bytes",
        "overall_peak_rss_bytes",
    ]

    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    df["timestamp_utc"] = pd.to_datetime(df["timestamp_utc"], errors="coerce", utc=True)
    return df


def write_summary(df: pd.DataFrame, outdir: Path) -> Path:
    metric_cols = [
        "load_ms", "query_total_ms", "avg_query_ms",
        "load_cpu_total_ms", "query_cpu_total_ms",
        "overall_peak_rss_bytes", "load_footprint_bytes",
    ]

    rows = []
    for metric in metric_cols:
        values = df[metric].dropna()
        if values.empty:
            continue
        rows.append({
            "metric": metric,
            "count": int(values.count()),
            "mean": values.mean(),
            "stddev": values.std(ddof=1) if values.count() > 1 else 0.0,
            "min": values.min(),
            "p50": values.quantile(0.50),
            "p95": values.quantile(0.95),
            "max": values.max(),
        })

    summary = pd.DataFrame(rows)
    path = outdir / "summary_stats.csv"
    summary.to_csv(path, index=False)
    return path


def save_svg(fig: plt.Figure, path: Path) -> None:
    fig.tight_layout()
    fig.savefig(path, format="svg")
    plt.close(fig)


def plot_time_trends(df: pd.DataFrame, outdir: Path) -> Path:
    fig, ax = plt.subplots(figsize=(11, 5))
    sns.lineplot(data=df, x="run", y="load_ms", marker="o", ax=ax, label="load_ms")
    sns.lineplot(data=df, x="run", y="avg_query_ms", marker="o", ax=ax, label="avg_query_ms")
    ax.set_title("Run Trend: Load vs Avg Query Time")
    ax.set_xlabel("Run")
    ax.set_ylabel("Milliseconds")
    ax.grid(alpha=0.25)
    path = outdir / "time_trends.svg"
    save_svg(fig, path)
    return path


def plot_cpu_trends(df: pd.DataFrame, outdir: Path) -> Path:
    fig, ax = plt.subplots(figsize=(11, 5))
    sns.lineplot(data=df, x="run", y="load_cpu_total_ms", marker="o", ax=ax, label="load_cpu_total_ms")
    sns.lineplot(data=df, x="run", y="query_cpu_total_ms", marker="o", ax=ax, label="query_cpu_total_ms")
    ax.set_title("Run Trend: CPU Time")
    ax.set_xlabel("Run")
    ax.set_ylabel("CPU ms")
    ax.grid(alpha=0.25)
    path = outdir / "cpu_trends.svg"
    save_svg(fig, path)
    return path


def plot_memory_trend(df: pd.DataFrame, outdir: Path) -> Path:
    memory_gb = df.copy()
    memory_gb["peak_rss_gb"] = memory_gb["overall_peak_rss_bytes"] / (1024 ** 3)

    has_footprint = "load_footprint_bytes" in memory_gb.columns and memory_gb["load_footprint_bytes"].notna().any()
    if has_footprint:
        memory_gb["load_footprint_gb"] = memory_gb["load_footprint_bytes"] / (1024 ** 3)
        memory_gb["query_footprint_gb"] = memory_gb["query_footprint_bytes"] / (1024 ** 3)

    fig, ax = plt.subplots(figsize=(11, 5))
    sns.lineplot(data=memory_gb, x="run", y="peak_rss_gb", marker="o", ax=ax, label="Peak RSS (getrusage)")
    if has_footprint:
        sns.lineplot(data=memory_gb, x="run", y="load_footprint_gb", marker="s", ax=ax, label="Load Footprint (task_vm_info)")
        sns.lineplot(data=memory_gb, x="run", y="query_footprint_gb", marker="^", ax=ax, label="Query Footprint (task_vm_info)")
    ax.set_title("Run Trend: Memory Usage")
    ax.set_xlabel("Run")
    ax.set_ylabel("Memory (GB)")
    ax.grid(alpha=0.25)
    path = outdir / "memory_trend.svg"
    save_svg(fig, path)
    return path


def plot_distributions(df: pd.DataFrame, outdir: Path) -> Path:
    dist_df = df[["load_ms", "avg_query_ms", "load_cpu_total_ms", "query_cpu_total_ms"]].copy()
    long_df = dist_df.melt(var_name="metric", value_name="value")

    fig, ax = plt.subplots(figsize=(11, 5))
    sns.boxplot(data=long_df, x="metric", y="value", ax=ax)
    ax.set_title("Metric Distributions")
    ax.set_xlabel("Metric")
    ax.set_ylabel("Value (ms)")
    ax.tick_params(axis="x", rotation=20)
    ax.grid(alpha=0.2)
    path = outdir / "distributions.svg"
    save_svg(fig, path)
    return path


def write_html_report(df: pd.DataFrame, summary_csv: Path, chart_paths: list[Path], outdir: Path) -> Path:
    mean_load = df["load_ms"].mean()
    mean_query = df["avg_query_ms"].mean()
    mean_cpu_load = df["load_cpu_total_ms"].mean()
    mean_cpu_query = df["query_cpu_total_ms"].mean()
    mean_peak_gb = df["overall_peak_rss_bytes"].mean() / (1024 ** 3)
    mean_footprint_gb = df["load_footprint_bytes"].mean() / (1024 ** 3) if "load_footprint_bytes" in df.columns else 0.0

    chart_imgs = "\n".join(
        f'<h3>{path.stem.replace("_", " ").title()}</h3>'
        f'<img src="{path.name}" style="max-width:100%;height:auto;" />'
        for path in chart_paths
    )

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>Phase 1 Benchmark Report</title>
  <style>
    body {{ font-family: Arial, sans-serif; margin: 24px; }}
    h1, h2 {{ margin-bottom: 8px; }}
    .kpi {{ margin: 4px 0; }}
    img {{ border: 1px solid #ddd; padding: 6px; margin-bottom: 18px; }}
    code {{ background: #f5f5f5; padding: 1px 4px; }}
  </style>
</head>
<body>
  <h1>Phase 1 Benchmark Report</h1>
  <p>Rows in dataset: <b>{int(df['total_rows'].iloc[0]) if not df.empty else 0}</b></p>
  <h2>Summary KPIs</h2>
  <div class="kpi">Mean load time: <b>{mean_load:.3f} ms</b></div>
  <div class="kpi">Mean avg-query time: <b>{mean_query:.3f} ms</b></div>
  <div class="kpi">Mean load CPU total: <b>{mean_cpu_load:.3f} ms</b></div>
  <div class="kpi">Mean query CPU total: <b>{mean_cpu_query:.3f} ms</b></div>
  <div class="kpi">Mean overall peak RSS: <b>{mean_peak_gb:.3f} GB</b></div>
  <div class="kpi">Mean phys footprint: <b>{mean_footprint_gb:.3f} GB</b></div>
  <p>Detailed stats: <code>{summary_csv.name}</code></p>
  <h2>Charts</h2>
  {chart_imgs}
</body>
</html>
"""

    path = outdir / "report.html"
    path.write_text(html, encoding="utf-8")
    return path


def main() -> None:
    args = parse_args()
    sns.set_theme(style="whitegrid")

    input_path = resolve_input_path(args.input)
    outdir = resolve_outdir_path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    df = load_data(input_path)
    if df.empty:
        raise SystemExit("Input CSV has no rows.")

    summary_csv = write_summary(df, outdir)
    charts = [
        plot_time_trends(df, outdir),
        plot_cpu_trends(df, outdir),
        plot_memory_trend(df, outdir),
        plot_distributions(df, outdir),
    ]
    report = write_html_report(df, summary_csv, charts, outdir)

    print(f"Wrote summary: {summary_csv}")
    for chart in charts:
        print(f"Wrote chart:   {chart}")
    print(f"Wrote report:  {report}")


if __name__ == "__main__":
    main()
