#!/usr/bin/env python3
"""
Process speed test logs found in stats/speedtests/ (files speed_<codec>_*.txt).
Outputs:
  - results/speed_results_raw.csv         (one row per binary per run)
  - results/speed_summary_by_codec.csv    (mean/median speeds & ratios per codec)
  - results/speed_speed_vs_ratio.png      (comp speed vs ratio, bubble)
  - results/speed_decomp_vs_ratio.png     (decomp speed vs ratio, bubble)
"""

import glob
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SPEED_DIR = os.path.join(ROOT, "stats", "speedtests")
OUT_DIR = os.path.join(ROOT, "stats", "results")

# Regex patterns
RE_CODEC = re.compile(r"Testing binary\s+\(-([^)]+)\):\s+(.+)")
RE_ORIG = re.compile(r"Original size:\s+([0-9]+)\s+bytes")
RE_PACK = re.compile(r"Packed size:\s+([0-9]+)\s+bytes")
RE_CT   = re.compile(r"Compression time:\s+([0-9.]+)")
RE_DT   = re.compile(r"Decompression time\(net\):\s+([0-9.]+)")
RE_RATIO= re.compile(r"Compression ratio:\s+([0-9.]+)")
RE_CS   = re.compile(r"Compression speed:\s+([0-9.+-eE]+)\s+MiB/s")
RE_DS   = re.compile(r"Decompression speed:\s+([0-9.+-eE]+)\s+MiB/s")


def parse_speedtests():
    rows = []
    files = sorted(glob.glob(os.path.join(SPEED_DIR, "speed_*.txt")))
    for path in files:
        with open(path, "r", errors="ignore") as f:
            lines = f.readlines()
        current = {
            "codec": None,
            "binary": None,
            "orig_bytes": None,
            "packed_bytes": None,
            "comp_time_s": None,
            "decomp_time_s": None,
            "ratio_pct": None,
            "comp_mib_s": None,
            "decomp_mib_s": None,
            "source_file": os.path.basename(path),
        }
        for line in lines:
            line = line.strip()
            if not line:
                continue
            m = RE_CODEC.search(line)
            if m:
                # flush previous if complete
                if current["codec"] and current["binary"]:
                    rows.append(current.copy())
                current = {
                    "codec": m.group(1),
                    "binary": m.group(2),
                    "orig_bytes": None,
                    "packed_bytes": None,
                    "comp_time_s": None,
                    "decomp_time_s": None,
                    "ratio_pct": None,
                    "comp_mib_s": None,
                    "decomp_mib_s": None,
                    "source_file": os.path.basename(path),
                }
                continue
            for (regex, key, cast) in [
                (RE_ORIG, "orig_bytes", int),
                (RE_PACK, "packed_bytes", int),
                (RE_CT, "comp_time_s", float),
                (RE_DT, "decomp_time_s", float),
                (RE_RATIO, "ratio_pct", float),
                (RE_CS, "comp_mib_s", float),
                (RE_DS, "decomp_mib_s", float),
            ]:
                m = regex.search(line)
                if m:
                    try:
                        current[key] = cast(m.group(1))
                    except Exception:
                        pass
        # flush last
        if current["codec"] and current["binary"]:
            rows.append(current.copy())
    return pd.DataFrame(rows)


def summarize(df: pd.DataFrame):
    agg = df.groupby("codec").agg(
        runs=("binary", "count"),
        comp_speed_mean=("comp_mib_s", "mean"),
        comp_speed_med=("comp_mib_s", "median"),
        decomp_speed_mean=("decomp_mib_s", "mean"),
        decomp_speed_med=("decomp_mib_s", "median"),
        ratio_mean=("ratio_pct", "mean"),
        ratio_med=("ratio_pct", "median"),
    ).reset_index()
    return agg


def plot_bubble(df: pd.DataFrame, xcol: str, ycol: str, fname: str, title: str):
    plt.figure(figsize=(12, 8))
    # scale bubble size by sqrt to keep visibility
    sizes = (df["orig_bytes"].fillna(0) / (1024 * 1024)).clip(lower=0.1) ** 0.5 * 50
    sns.scatterplot(
        data=df,
        x=xcol,
        y=ycol,
        hue="codec",
        size=sizes,
        sizes=(20, 300),
        alpha=0.7,
    )
    plt.xlabel(xcol.replace("_", " "))
    plt.ylabel(ycol.replace("_", " "))
    plt.title(title)
    plt.grid(alpha=0.3)
    plt.tight_layout()
    out = os.path.join(OUT_DIR, fname)
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"    ✓ Generated {fname}")


def remove_outliers_iqr(df: pd.DataFrame, cols, fence: float = 1.5):
    """Remove outliers per codec using IQR fence on specified columns."""
    masks = []
    for codec, sub in df.groupby("codec"):
        sub_mask = pd.Series(True, index=sub.index)
        for col in cols:
            if col not in sub or sub[col].isna().all():
                continue
            q1 = sub[col].quantile(0.25)
            q3 = sub[col].quantile(0.75)
            iqr = q3 - q1
            if iqr == 0:
                continue
            lower = q1 - fence * iqr
            upper = q3 + fence * iqr
            sub_mask &= sub[col].between(lower, upper, inclusive="both")
        masks.append(sub_mask)
    if not masks:
        return df, {}
    mask_all = pd.concat(masks).sort_index()
    filtered = df.loc[mask_all]
    removed_counts = (
        df.assign(keep=mask_all)
        .groupby("codec")["keep"]
        .apply(lambda s: (~s).sum())
        .to_dict()
    )
    return filtered, removed_counts


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    df = parse_speedtests()
    if df.empty:
        print("No speed test data found.")
        return

    raw_path = os.path.join(OUT_DIR, "speed_results_raw.csv")
    df.to_csv(raw_path, index=False)
    print(f"✓ Saved raw speed results: {raw_path} (rows={len(df)})")

    # Filter outliers per codec on compression/decompression speed
    df_filt, removed = remove_outliers_iqr(df, cols=["comp_mib_s", "decomp_mib_s"], fence=1.5)
    if removed:
        print("Outliers removed per codec (IQR 1.5x fence):")
        for k, v in sorted(removed.items()):
            if v:
                print(f"  - {k}: {v} rows removed")
    else:
        print("No outliers removed.")

    filt_path = os.path.join(OUT_DIR, "speed_results_filtered.csv")
    df_filt.to_csv(filt_path, index=False)
    print(f"✓ Saved filtered speed results: {filt_path} (rows={len(df_filt)})")

    summary = summarize(df_filt)
    summary_path = os.path.join(OUT_DIR, "speed_summary_by_codec.csv")
    summary.to_csv(summary_path, index=False)
    print(f"✓ Saved speed summary (filtered): {summary_path}")

    # Plots
    plot_bubble(
        df_filt,
        xcol="comp_mib_s",
        ycol="ratio_pct",
        fname="speed_speed_vs_ratio.png",
        title="Compression speed vs ratio (bubble size ~ sqrt(file size MB))",
    )
    plot_bubble(
        df_filt,
        xcol="decomp_mib_s",
        ycol="ratio_pct",
        fname="speed_decomp_vs_ratio.png",
        title="Decompression speed vs ratio (bubble size ~ sqrt(file size MB))",
    )


if __name__ == "__main__":
    main()

