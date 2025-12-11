#!/usr/bin/env python3
"""
Binary Compression Dataset Analyzer
Analyzes compression reports to extract statistics and create a predictor.
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import os
import glob
from pathlib import Path
import json

# Determine the repository root (assuming this script is in stats/)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_DATASETS_DIR = os.path.join(REPO_ROOT, 'build', 'predictor_datasets')
DEFAULT_STUBS_DIR = os.path.join(REPO_ROOT, 'build', 'stubs')

# ZSTD stub size constraint: ZSTD has a large stub (~20-25KB), making it impractical
# for small binaries. We exclude binaries < 50KB for ZSTD in all analyses.
ZSTD_MIN_BINARY_SIZE = 50 * 1024  # 50KB in bytes

# Additional codec-specific size constraints (for size-range rankings/plots)
EXO_MAX_SIZE = 2 * 1024 * 1024   # 2 MB: EXO uses too much RAM beyond this
ZX0_MAX_SIZE = 3 * 1024 * 1024   # 3 MB: ZX0 is very slow beyond this

# Style configuration for plots
sns.set_theme(style="whitegrid")
plt.rcParams['figure.figsize'] = (12, 8)


def apply_codec_size_exclusions(comp_name: str, df: pd.DataFrame) -> pd.DataFrame:
    """
    Apply codec-specific size constraints for size-range analyses.
    - ZSTD: exclude < 50KB (stub overhead)
    - EXO:  exclude > 2MB (RAM use)
    - ZX0:  exclude > 3MB (very slow)
    """
    if df is None or df.empty or 'file_size' not in df.columns:
        return df
    res = df
    if comp_name == 'zstd':
        res = res[res['file_size'] >= ZSTD_MIN_BINARY_SIZE]
    if comp_name == 'exo':
        res = res[res['file_size'] <= EXO_MAX_SIZE]
    if comp_name == 'zx0':
        res = res[res['file_size'] <= ZX0_MAX_SIZE]
    return res

class CompressorAnalyzer:
    """Analyzes compression datasets for different compressors"""

    def __init__(self, datasets_dir=None):
        if datasets_dir is None:
            datasets_dir = DEFAULT_DATASETS_DIR
        self.datasets_dir = datasets_dir
        self.compressors = {}
        self.size_ranges = [
            (0, 20000, '<20KB'),
            (20001, 200000, '20-200KB'),
            (200001, 500000, '200-500KB'),
            (500001, float('inf'), '>500KB')
        ]

    def load_datasets(self):
        """Loads all CSV datasets"""
        print(f"Loading datasets from: {self.datasets_dir}")
        if not os.path.isdir(self.datasets_dir):
            print(f"Error: Datasets directory does not exist: {self.datasets_dir}")
            print("Please ensure you have collected datasets using tools/collect_codec_deep.sh")
            return self
        
        csv_files = glob.glob(os.path.join(self.datasets_dir, '*_deep.csv'))

        for csv_file in csv_files:
            # Extract compressor name from the file
            compressor_name = Path(csv_file).stem.replace('_deep', '')
            print(f"  - Loading {compressor_name}...")

            try:
                df = pd.read_csv(csv_file)
                # Add a column with the compressor name
                df['compressor'] = compressor_name
                
                # Calculate ratios if they are missing but final sizes are available
                if 'bcj_ratio' in df.columns and df['bcj_ratio'].isna().all():
                    if 'bcj_final' in df.columns and 'file_size' in df.columns:
                        mask = df['bcj_final'].notna() & df['file_size'].notna() & (df['file_size'] > 0)
                        df.loc[mask, 'bcj_ratio'] = (df.loc[mask, 'bcj_final'] / df.loc[mask, 'file_size']) * 100
                        print(f"    → Calculated bcj_ratio from bcj_final")
                
                if 'kanzi_ratio' in df.columns and df['kanzi_ratio'].isna().all():
                    if 'kanzi_final' in df.columns and 'file_size' in df.columns:
                        mask = df['kanzi_final'].notna() & df['file_size'].notna() & (df['file_size'] > 0)
                        df.loc[mask, 'kanzi_ratio'] = (df.loc[mask, 'kanzi_final'] / df.loc[mask, 'file_size']) * 100
                        print(f"    → Calculated kanzi_ratio from kanzi_final")
                
                self.compressors[compressor_name] = df
                print(f"    ✓ {len(df)} binaries loaded")
            except Exception as e:
                print(f"    ✗ Error: {e}")

        print(f"\n{len(self.compressors)} compressors loaded in total.\n")
        return self

    def calculate_compression_ratios(self):
        """Calculates the average compression ratios for each compressor"""
        print("=" * 80)
        print("ANALYSIS OF AVERAGE COMPRESSION RATIOS")
        print("=" * 80)

        results = []

        for comp_name, df in self.compressors.items():
            # Use the bcj_ratio and kanzi_ratio columns which are already calculated
            if 'bcj_ratio' in df.columns and 'kanzi_ratio' in df.columns:
                # ZSTD exclusion: filter out binaries < 50KB for ZSTD due to large stub size
                if comp_name == 'zstd' and 'file_size' in df.columns:
                    df = df[df['file_size'] >= ZSTD_MIN_BINARY_SIZE].copy()
                
                # Filter valid (non-NaN) values
                valid_bcj = df['bcj_ratio'].dropna()
                valid_kanzi = df['kanzi_ratio'].dropna()
                
                # Skip if no valid data after filtering
                if len(valid_bcj) == 0 or len(valid_kanzi) == 0:
                    continue

                avg_bcj_ratio = valid_bcj.mean()
                avg_kanzi_ratio = valid_kanzi.mean()

                # Take the best ratio (the smallest = better compression)
                best_ratio = min(avg_bcj_ratio, avg_kanzi_ratio)
                best_filter = 'BCJ' if avg_bcj_ratio < avg_kanzi_ratio else 'KanziEXE'

                results.append({
                    'Compressor': comp_name,
                    'Average BCJ Ratio (%)': round(avg_bcj_ratio, 2),
                    'Average KanziEXE Ratio (%)': round(avg_kanzi_ratio, 2),
                    'Best Ratio (%)': round(best_ratio, 2),
                    'Best Filter': best_filter,
                    'Binary Count': len(valid_bcj)
                })

        # Create a DataFrame and sort by best ratio
        results_df = pd.DataFrame(results).sort_values('Best Ratio (%)')

        print("\nRanking of compressors by average compression ratio:")
        print("Note: ZSTD excludes binaries < 50KB due to large stub size (~20-25KB)")
        print("      ZSTD is reserved for binaries >= 50KB where stub overhead is negligible")
        print()
        print(results_df.to_string(index=False))
        print()

        # Save the results
        results_path = os.path.join(SCRIPT_DIR, 'results', 'average_compression_ratios.csv')
        results_df.to_csv(results_path, index=False)
        print(f"✓ Results saved in: {results_path}\n")

        return results_df

    def rank_by_size_ranges(self):
        """Ranks the best compressors for each size range (including granular ranges)"""
        print("=" * 80)
        print("RANKING OF COMPRESSORS BY SIZE RANGE")
        print("=" * 80)

        all_results = []

        # Define all ranges: broad ranges + granular ranges
        all_ranges = [
            # Broad ranges (for console display)
            *self.size_ranges,
            # Granular ranges (for detailed CSV)
            # Up to 200KB
            (0, 15 * 1024, '<15KB'),
            (15 * 1024, 50 * 1024, '15-50KB'),
            (50 * 1024, 100 * 1024, '50-100KB'),
            (100 * 1024, 200 * 1024, '100-200KB'),
            # 200KB to 500KB
            (200 * 1024, 300 * 1024, '200-300KB'),
            (300 * 1024, 400 * 1024, '300-400KB'),
            (400 * 1024, 450 * 1024, '400-450KB'),
            (450 * 1024, 500 * 1024, '450-500KB'),
            # 500KB to 1500KB
            (500 * 1024, 600 * 1024, '500-600KB'),
            (600 * 1024, 800 * 1024, '600-800KB'),
            (800 * 1024, 1000 * 1024, '800KB-1MB'),
            (1000 * 1024, 1500 * 1024, '1-1.5MB'),
            # 1.5MB to 15MB
            (1500 * 1024, 4000 * 1024, '1.5-4MB'),
            (4000 * 1024, 7500 * 1024, '4-7.5MB'),
            (7500 * 1024, 12000 * 1024, '7.5-12MB'),
            (12000 * 1024, 15000 * 1024, '12-15MB'),
            # Above 15MB
            (15000 * 1024, 20000 * 1024, '15-20MB'),
            (20000 * 1024, 40000 * 1024, '20-40MB'),
            (40000 * 1024, 50000 * 1024, '40-50MB'),
            (50000 * 1024, float('inf'), '>50MB'),
        ]

        # Track which ranges we've printed to console (only broad ranges)
        printed_ranges = set()

        for min_size, max_size, label in all_ranges:
            comp_rankings = []

            for comp_name, df in self.compressors.items():
                # Filter by size
                if max_size == float('inf'):
                    mask = df['file_size'] >= min_size
                else:
                    mask = (df['file_size'] >= min_size) & (df['file_size'] < max_size)
                filtered_df = df[mask]
                filtered_df = apply_codec_size_exclusions(comp_name, filtered_df)

                if len(filtered_df) == 0:
                    continue

                # Calculate the best ratio for this range
                if 'bcj_ratio' in filtered_df.columns and 'kanzi_ratio' in filtered_df.columns:
                    # For each binary, take the best ratio (BCJ or Kanzi)
                    best_ratios = filtered_df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                    best_ratios = best_ratios[~(np.isnan(best_ratios) | np.isinf(best_ratios))]
                    if len(best_ratios) == 0:
                        continue
                    avg_best_ratio = best_ratios.mean()

                    comp_rankings.append({
                        'Compressor': comp_name,
                        'Average Ratio (%)': round(avg_best_ratio, 2),
                        'Binary Count': len(filtered_df)
                    })

            if not comp_rankings:
                continue

            # Sort and get top 10 (for CSV) or top 5 (for console display)
            rankings_df = pd.DataFrame(comp_rankings).sort_values('Average Ratio (%)')
            top_for_csv = rankings_df.head(10)
            top_for_display = rankings_df.head(5)

            # Print to console only for broad ranges
            if label in [r[2] for r in self.size_ranges] and label not in printed_ranges:
                print(f"\n{label} ({min_size:,} - {max_size:,} bytes)")
                print("-" * 60)
                print(top_for_display.to_string(index=False))
                printed_ranges.add(label)

            # Save for CSV (all ranges, top 10)
            for rank, (idx, row) in enumerate(top_for_csv.iterrows(), start=1):
                all_results.append({
                    'Range': label,
                    'Rank': rank,
                    'Compressor': row['Compressor'],
                    'Average Ratio (%)': row['Average Ratio (%)'],
                    'Binary Count': row['Binary Count']
                })

        # Save all results to CSV
        results_df = pd.DataFrame(all_results)
        results_path = os.path.join(SCRIPT_DIR, 'results', 'rankings_by_size.csv')
        results_df.to_csv(results_path, index=False)
        print(f"\n✓ Results saved in: {results_path}")
        print(f"  Total ranges analyzed: {results_df['Range'].nunique()}")
        print(f"  Total entries: {len(results_df)}\n")

        return results_df

    def create_visualizations(self):
        """Creates graphs and visualizations"""
        print("=" * 80)
        print("GENERATING VISUALIZATIONS")
        print("=" * 80)

        # 1. Graph of average ratios by compressor
        self._plot_average_ratios()

        # 2. Graphs of performance by size range (legacy - 4 broad ranges)
        self._plot_size_range_performance()

        # 2b. Granular graphs of performance by detailed size ranges
        self._plot_granular_size_ranges()

        # 3. BCJ vs KanziEXE comparison
        self._plot_filter_comparison()

        # 4. Distribution of compression ratios
        self._plot_ratio_distributions()

        # 5. Performance curves as a function of size
        self._plot_performance_curves()

        # 6. Stub overhead impact
        self._plot_stub_overhead()

        results_dir = os.path.join(SCRIPT_DIR, 'results')
        print(f"\n✓ All visualizations generated in: {results_dir}/\n")

    def _plot_average_ratios(self):
        """Graph of average ratios"""
        print("  - Generating the average ratios graph...")

        data = []
        for comp_name, df in self.compressors.items():
            if 'bcj_ratio' in df.columns:
                best_ratios = df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                data.append({
                    'Compressor': comp_name,
                    'Average Ratio': best_ratios.mean()
                })

        plot_df = pd.DataFrame(data).sort_values('Average Ratio')

        plt.figure(figsize=(14, 8))
        bars = plt.barh(plot_df['Compressor'], plot_df['Average Ratio'])

        # Color the bars according to the ratio
        colors = plt.cm.RdYlGn_r(plot_df['Average Ratio'] / plot_df['Average Ratio'].max())
        for bar, color in zip(bars, colors):
            bar.set_color(color)

        plt.xlabel('Average compression ratio (%)', fontsize=12)
        plt.ylabel('Compressor', fontsize=12)
        plt.title('Comparison of average compression ratios by compressor\n(smaller = better)',
                  fontsize=14, fontweight='bold')
        plt.grid(axis='x', alpha=0.3)

        # Add values on the bars
        for i, v in enumerate(plot_df['Average Ratio']):
            plt.text(v + 0.5, i, f'{v:.1f}%', va='center', fontsize=10)

        plt.tight_layout()
        output_path = os.path.join(SCRIPT_DIR, 'results', 'average_ratios.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()

    def _plot_size_range_performance(self):
        """Graph of performance by range (legacy - 4 broad ranges)"""
        print("  - Generating the graph by size range...")

        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        axes = axes.flatten()

        for idx, (min_size, max_size, label) in enumerate(self.size_ranges):
            ax = axes[idx]

            data = []
            for comp_name, df in self.compressors.items():
                mask = (df['file_size'] >= min_size) & (df['file_size'] <= max_size)
                filtered_df = apply_codec_size_exclusions(comp_name, df[mask])

                if len(filtered_df) > 0 and 'bcj_ratio' in filtered_df.columns:
                    best_ratios = filtered_df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                    best_ratios = best_ratios[~(np.isnan(best_ratios) | np.isinf(best_ratios))]
                    if len(best_ratios) > 0:
                        data.append({
                            'Compressor': comp_name,
                            'Ratio': best_ratios.mean()
                        })

            if data:
                plot_df = pd.DataFrame(data).sort_values('Ratio').head(10)

                bars = ax.barh(plot_df['Compressor'], plot_df['Ratio'])
                colors = plt.cm.RdYlGn_r(plot_df['Ratio'] / plot_df['Ratio'].max())
                for bar, color in zip(bars, colors):
                    bar.set_color(color)

                ax.set_xlabel('Average ratio (%)', fontsize=10)
                ax.set_title(f'{label}', fontsize=12, fontweight='bold')
                ax.grid(axis='x', alpha=0.3)

                for i, v in enumerate(plot_df['Ratio']):
                    ax.text(v + 0.5, i, f'{v:.1f}%', va='center', fontsize=9)

        plt.suptitle('Top 10 compressors by binary size range\n(Note: ZSTD excluded for binaries < 50KB due to large stub size ~20-25KB)',
                     fontsize=16, fontweight='bold')
        plt.tight_layout()
        output_path = os.path.join(SCRIPT_DIR, 'results', 'performance_by_size_range.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()

    def _plot_granular_size_ranges(self):
        """Generate granular performance graphs by detailed size ranges"""
        print("  - Generating granular size range graphs...")

        # Define granular size ranges
        granular_ranges = [
            # Up to 200KB
            {
                'file': 'performance_by_size_range_up_to_200K.png',
                'ranges': [
                    (0, 15 * 1024, '<15KB'),
                    (15 * 1024, 50 * 1024, '15-50KB'),
                    (50 * 1024, 100 * 1024, '50-100KB'),
                    (100 * 1024, 200 * 1024, '100-200KB'),
                ]
            },
            # 200KB to 500KB
            {
                'file': 'performance_by_size_range_up_to_500K.png',
                'ranges': [
                    (200 * 1024, 300 * 1024, '200-300KB'),
                    (300 * 1024, 400 * 1024, '300-400KB'),
                    (400 * 1024, 450 * 1024, '400-450KB'),
                    (450 * 1024, 500 * 1024, '450-500KB'),
                ]
            },
            # 500KB to 1500KB
            {
                'file': 'performance_by_size_range_up_to_1500K.png',
                'ranges': [
                    (500 * 1024, 600 * 1024, '500-600KB'),
                    (600 * 1024, 800 * 1024, '600-800KB'),
                    (800 * 1024, 1000 * 1024, '800KB-1MB'),
                    (1000 * 1024, 1500 * 1024, '1-1.5MB'),
                ]
            },
            # 1.5MB to 15MB
            {
                'file': 'performance_by_size_range_up_to_15000K.png',
                'ranges': [
                    (1500 * 1024, 4000 * 1024, '1.5-4MB'),
                    (4000 * 1024, 7500 * 1024, '4-7.5MB'),
                    (7500 * 1024, 12000 * 1024, '7.5-12MB'),
                    (12000 * 1024, 15000 * 1024, '12-15MB'),
                ]
            },
            # Above 15MB
            {
                'file': 'performance_by_size_range_up_to_max.png',
                'ranges': [
                    (15000 * 1024, 20000 * 1024, '15-20MB'),
                    (20000 * 1024, 40000 * 1024, '20-40MB'),
                    (40000 * 1024, 50000 * 1024, '40-50MB'),
                    (50000 * 1024, float('inf'), '>50MB'),
                ]
            },
        ]

        for group in granular_ranges:
            fig, axes = plt.subplots(2, 2, figsize=(16, 12))
            axes = axes.flatten()

            for idx, (min_size, max_size, label) in enumerate(group['ranges']):
                ax = axes[idx]

                data = []
                for comp_name, df in self.compressors.items():
                    if max_size == float('inf'):
                        mask = df['file_size'] >= min_size
                    else:
                        mask = (df['file_size'] >= min_size) & (df['file_size'] < max_size)
                    filtered_df = apply_codec_size_exclusions(comp_name, df[mask])

                    if len(filtered_df) > 0 and 'bcj_ratio' in filtered_df.columns:
                        best_ratios = filtered_df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                        best_ratios = best_ratios[~(np.isnan(best_ratios) | np.isinf(best_ratios))]
                        if len(best_ratios) > 0:
                            data.append({
                                'Compressor': comp_name,
                                'Ratio': best_ratios.mean(),
                                'Count': len(best_ratios)
                            })

                if data:
                    plot_df = pd.DataFrame(data).sort_values('Ratio').head(10)

                    bars = ax.barh(plot_df['Compressor'], plot_df['Ratio'])
                    colors = plt.cm.RdYlGn_r(plot_df['Ratio'] / plot_df['Ratio'].max())
                    for bar, color in zip(bars, colors):
                        bar.set_color(color)

                    ax.set_xlabel('Average ratio (%)', fontsize=10)
                    ax.set_title(f'{label} (n={plot_df["Count"].sum()})', fontsize=12, fontweight='bold')
                    ax.grid(axis='x', alpha=0.3)

                    for i, v in enumerate(plot_df['Ratio']):
                        ax.text(v + 0.5, i, f'{v:.1f}%', va='center', fontsize=9)
                else:
                    ax.text(0.5, 0.5, 'No data', ha='center', va='center', transform=ax.transAxes)
                    ax.set_title(f'{label}', fontsize=12, fontweight='bold')

            title_suffix = ' (ZSTD excluded for < 50KB)' if any(r[0] < ZSTD_MIN_BINARY_SIZE for r in group['ranges']) else ''
            plt.suptitle(f'Top 10 compressors by binary size range{title_suffix}',
                         fontsize=16, fontweight='bold')
            plt.tight_layout()
            output_path = os.path.join(SCRIPT_DIR, 'results', group['file'])
            plt.savefig(output_path, dpi=300, bbox_inches='tight')
            plt.close()
            print(f"    ✓ Generated {group['file']}")

    def _plot_filter_comparison(self):
        """BCJ vs KanziEXE comparison"""
        print("  - Generating the filter comparison graph...")

        data = []
        for comp_name, df in self.compressors.items():
            if 'bcj_ratio' in df.columns and 'kanzi_ratio' in df.columns:
                data.append({
                    'Compressor': comp_name,
                    'BCJ': df['bcj_ratio'].mean(),
                    'KanziEXE': df['kanzi_ratio'].mean()
                })

        plot_df = pd.DataFrame(data).sort_values('BCJ')

        fig, ax = plt.subplots(figsize=(14, 8))

        x = np.arange(len(plot_df))
        width = 0.35

        bars1 = ax.bar(x - width/2, plot_df['BCJ'], width, label='BCJ', alpha=0.8)
        bars2 = ax.bar(x + width/2, plot_df['KanziEXE'], width, label='KanziEXE', alpha=0.8)

        ax.set_xlabel('Compressor', fontsize=12)
        ax.set_ylabel('Average ratio (%)', fontsize=12)
        ax.set_title('BCJ vs KanziEXE comparison for each compressor',
                     fontsize=14, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(plot_df['Compressor'], rotation=45, ha='right')
        ax.legend()
        ax.grid(axis='y', alpha=0.3)

        plt.tight_layout()
        output_path = os.path.join(SCRIPT_DIR, 'results', 'bcj_vs_kanzi.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()

    def _plot_ratio_distributions(self):
        """Distribution of ratios"""
        print("  - Generating ratio distributions...")

        fig, axes = plt.subplots(3, 5, figsize=(20, 12))
        axes = axes.flatten()

        for idx, (comp_name, df) in enumerate(sorted(self.compressors.items())):
            if idx >= len(axes):
                break

            ax = axes[idx]

            if 'bcj_ratio' in df.columns and 'kanzi_ratio' in df.columns:
                best_ratios = df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                # Filter out NaN and infinite values
                best_ratios = best_ratios[~(np.isnan(best_ratios) | np.isinf(best_ratios))]

                if len(best_ratios) > 0:
                    ax.hist(best_ratios, bins=30, alpha=0.7, edgecolor='black')
                    mean_ratio = best_ratios.mean()
                    if not (np.isnan(mean_ratio) or np.isinf(mean_ratio)):
                        ax.axvline(mean_ratio, color='red', linestyle='--',
                                  linewidth=2, label=f'Average: {mean_ratio:.1f}%')
                    ax.set_xlabel('Ratio (%)', fontsize=9)
                    ax.set_ylabel('Frequency', fontsize=9)
                    ax.set_title(f'{comp_name}', fontsize=10, fontweight='bold')
                    ax.legend(fontsize=8)
                    ax.grid(alpha=0.3)
                else:
                    ax.text(0.5, 0.5, 'No valid data', ha='center', va='center', transform=ax.transAxes)
                    ax.set_title(f'{comp_name}', fontsize=10, fontweight='bold')
            else:
                ax.text(0.5, 0.5, 'No data', ha='center', va='center', transform=ax.transAxes)
                ax.set_title(f'{comp_name}', fontsize=10, fontweight='bold')

        # Hide unused axes
        for idx in range(len(self.compressors), len(axes)):
            axes[idx].axis('off')

        plt.suptitle('Distribution of compression ratios by compressor',
                     fontsize=16, fontweight='bold')
        plt.tight_layout()
        output_path = os.path.join(SCRIPT_DIR, 'results', 'ratio_distributions.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()

    def _plot_performance_curves(self):
        """Performance curves as a function of size"""
        print("  - Generating performance curves...")

        plt.figure(figsize=(16, 10))

        # For each compressor, calculate the average ratio by size bins
        size_bins = np.logspace(3, 7, 50)  # From 1KB to 10MB

        for comp_name, df in sorted(self.compressors.items()):
            if 'bcj_ratio' not in df.columns or 'kanzi_ratio' not in df.columns:
                continue

            # ZSTD exclusion: filter out binaries < 50KB for ZSTD
            if comp_name == 'zstd' and 'file_size' in df.columns:
                df = df[df['file_size'] >= ZSTD_MIN_BINARY_SIZE].copy()

            ratios_by_size = []
            sizes = []

            for i in range(len(size_bins) - 1):
                mask = (df['file_size'] >= size_bins[i]) & (df['file_size'] < size_bins[i+1])
                filtered = df[mask]

                if len(filtered) > 0:
                    best_ratios = filtered[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                    # Filter out NaN and infinite values
                    best_ratios = best_ratios[~(np.isnan(best_ratios) | np.isinf(best_ratios))]
                    if len(best_ratios) > 0:
                        mean_ratio = best_ratios.mean()
                        if not (np.isnan(mean_ratio) or np.isinf(mean_ratio)):
                            ratios_by_size.append(mean_ratio)
                            sizes.append((size_bins[i] + size_bins[i+1]) / 2)

            if sizes and len(sizes) > 0:
                plt.plot(sizes, ratios_by_size, marker='o', label=comp_name, alpha=0.7, linewidth=2)

        plt.xlabel('Binary size (bytes)', fontsize=12)
        plt.ylabel('Average compression ratio (%)', fontsize=12)
        plt.title('Compressor performance as a function of binary size',
                  fontsize=14, fontweight='bold')
        plt.xscale('log')
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=10)
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        output_path = os.path.join(SCRIPT_DIR, 'results', 'performance_curves.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()

    # --- Stub overhead impact ---
    def _plot_stub_overhead(self):
        """Plot stub sizes per compressor to visualize overhead."""
        print("  - Generating stub overhead chart...")

        stubs_dir = DEFAULT_STUBS_DIR
        if not os.path.isdir(stubs_dir):
            print(f"    ⚠ Stubs directory not found: {stubs_dir}")
            return

        codecs = sorted(self.compressors.keys())

        def find_stub_size(codec, kind, filt):
            """
            codec: str, kind: 'dynamic'|'static', filt: 'bcj'|'kexe'
            Prefer non-password stubs; fall back to any if needed.
            """
            patterns = [
                f"stub_{codec}_{kind}_{filt}.bin",
                f"stub_{codec}_{kind}_{filt}_pwd.bin",
                f"stub_{codec}_{kind}_{filt}*.bin",
            ]
            for pat in patterns:
                matches = glob.glob(os.path.join(stubs_dir, pat))
                if matches:
                    # Take the smallest to stay conservative
                    sizes = [os.path.getsize(m) for m in matches]
                    return min(sizes)
            return None

        rows = []
        for codec in codecs:
            dyn_bcj = find_stub_size(codec, "dynamic", "bcj")
            dyn_kexe = find_stub_size(codec, "dynamic", "kexe")
            sta_bcj = find_stub_size(codec, "static", "bcj")
            sta_kexe = find_stub_size(codec, "static", "kexe")

            if not any([dyn_bcj, dyn_kexe, sta_bcj, sta_kexe]):
                continue

            rows.append({
                "Compressor": codec,
                "dynamic_bcj": dyn_bcj,
                "dynamic_kexe": dyn_kexe,
                "static_bcj": sta_bcj,
                "static_kexe": sta_kexe,
            })

        if not rows:
            print("    ⚠ No stub sizes found.")
            return

        df = pd.DataFrame(rows)
        # Convert to KB for readability
        for col in ["dynamic_bcj", "dynamic_kexe", "static_bcj", "static_kexe"]:
            if col in df:
                df[col] = df[col].astype(float) / 1024.0

        df_long = df.melt(id_vars=["Compressor"], var_name="StubType", value_name="SizeKB")
        df_long = df_long.dropna()

        plt.figure(figsize=(14, 8))
        order = df["Compressor"].tolist()
        sns.barplot(data=df_long, x="SizeKB", y="Compressor", hue="StubType", order=order, orient="h")
        plt.xlabel("Stub size (KB)")
        plt.ylabel("Compressor")
        plt.title("Stub overhead by compressor (BCJ vs Kanzi, dynamic vs static)\nNote: ZSTD excluded <50KB elsewhere due to large stub (~20-25KB)")
        plt.legend(title="Stub type", bbox_to_anchor=(1.02, 1), loc="upper left")
        plt.tight_layout()

        output_path = os.path.join(SCRIPT_DIR, 'results', 'stub_overhead.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"    ✓ Generated stub_overhead.png")

    def create_predictor(self):
        """Creates a simple predictor to choose the best compressor"""
        print("=" * 80)
        print("CREATING THE COMPRESSOR PREDICTOR")
        print("=" * 80)

        predictor_rules = {
            'by_size': {},
            'by_entropy': {},
            'global_recommendation': None
        }

        # Rule 1: By binary size range
        print("\nRules based on binary size:")
        print("-" * 60)

        for min_size, max_size, label in self.size_ranges:
            best_comp = None
            best_ratio = float('inf')

            for comp_name, df in self.compressors.items():
                mask = (df['file_size'] >= min_size) & (df['file_size'] <= max_size)
                filtered_df = df[mask]

                if len(filtered_df) > 0 and 'bcj_ratio' in filtered_df.columns:
                    best_ratios = filtered_df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                    avg_ratio = best_ratios.mean()

                    if avg_ratio < best_ratio:
                        best_ratio = avg_ratio
                        best_comp = comp_name

            predictor_rules['by_size'][label] = {
                'compressor': best_comp,
                'ratio': round(best_ratio, 2),
                'range': (min_size, max_size)
            }

            print(f"  {label:15} → {best_comp:12} (ratio: {best_ratio:.2f}%)")

        # Best overall compressor
        global_best = None
        global_best_ratio = float('inf')

        for comp_name, df in self.compressors.items():
            if 'bcj_ratio' in df.columns:
                best_ratios = df[['bcj_ratio', 'kanzi_ratio']].min(axis=1)
                avg_ratio = best_ratios.mean()

                if avg_ratio < global_best_ratio:
                    global_best_ratio = avg_ratio
                    global_best = comp_name

        predictor_rules['global_recommendation'] = {
            'compressor': global_best,
            'ratio': round(global_best_ratio, 2)
        }

        print(f"\nBest overall compressor: {global_best} (average ratio: {global_best_ratio:.2f}%)")

        # Save the rules
        rules_path = os.path.join(SCRIPT_DIR, 'results', 'predictor_rules.json')
        with open(rules_path, 'w') as f:
            json.dump(predictor_rules, f, indent=2)

        print(f"\n✓ Predictor rules saved in: {rules_path}\n")

        # Create a predictor function
        self._create_predictor_script(predictor_rules)

        return predictor_rules

    def _create_predictor_script(self, rules):
        """Creates a Python script to use the predictor"""

        # Convert rules to a pure Python compatible format
        by_size_rules = {}
        for label, rule in rules['by_size'].items():
            by_size_rules[label] = {
                'compressor': rule['compressor'],
                'ratio': float(rule['ratio']),
                'range': (int(rule['range'][0]),
                         float('inf') if rule['range'][1] == float('inf') else int(rule['range'][1]))
            }

        script_content = f'''#!/usr/bin/env python3
"""
Optimal Compressor Predictor
Automatically generated by analyze_compressors.py
"""

def predict_best_compressor(file_size):
    """
    Predicts the best compressor based on file size

    Args:
        file_size: File size in bytes

    Returns:
        tuple: (compressor_name, expected_ratio)
    """
    rules = {{
        '<20KB': {{'compressor': '{by_size_rules["<20KB"]["compressor"]}', 'ratio': {by_size_rules["<20KB"]["ratio"]}, 'range': ({by_size_rules["<20KB"]["range"][0]}, {by_size_rules["<20KB"]["range"][1]})}},
        '20-200KB': {{'compressor': '{by_size_rules["20-200KB"]["compressor"]}', 'ratio': {by_size_rules["20-200KB"]["ratio"]}, 'range': ({by_size_rules["20-200KB"]["range"][0]}, {by_size_rules["20-200KB"]["range"][1]})}},
        '200-500KB': {{'compressor': '{by_size_rules["200-500KB"]["compressor"]}', 'ratio': {by_size_rules["200-500KB"]["ratio"]}, 'range': ({by_size_rules["200-500KB"]["range"][0]}, {by_size_rules["200-500KB"]["range"][1]})}},
        '>500KB': {{'compressor': '{by_size_rules[">500KB"]["compressor"]}', 'ratio': {by_size_rules[">500KB"]["ratio"]}, 'range': ({by_size_rules[">500KB"]["range"][0]}, float('inf'))}}
    }}

    for size_range, rule in rules.items():
        min_size, max_size = rule['range']
        if min_size <= file_size <= max_size:
            return rule['compressor'], rule['ratio']

    # Default to the global best
    return "{rules['global_recommendation']['compressor']}", {float(rules['global_recommendation']['ratio'])}

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python predictor.py <file_size_in_bytes>")
        sys.exit(1)

    size = int(sys.argv[1])
    compressor, ratio = predict_best_compressor(size)

    print(f"For a file of {{size:,}} bytes:")
    print(f"  Recommended compressor: {{compressor}}")
    print(f"  Expected ratio: {{ratio}}%")
'''

        predictor_path = os.path.join(SCRIPT_DIR, 'results', 'predictor.py')
        with open(predictor_path, 'w') as f:
            f.write(script_content)

        os.chmod(predictor_path, 0o755)
        print(f"✓ Predictor script created: {predictor_path}")


def main():
    """Main function"""
    print("\n" + "=" * 80)
    print("COMPRESSION DATASET ANALYZER")
    print("=" * 80 + "\n")

    # Create the results folder if it doesn't exist
    results_dir = os.path.join(SCRIPT_DIR, 'results')
    os.makedirs(results_dir, exist_ok=True)

    # Initialize the analyzer (reads from build/predictor_datasets/)
    analyzer = CompressorAnalyzer()  # Uses DEFAULT_DATASETS_DIR

    # Load the datasets
    analyzer.load_datasets()

    # Perform the analyses
    print("\n" + "=" * 80)
    print("STARTING ANALYSES")
    print("=" * 80 + "\n")

    # 1. Calculate average ratios
    analyzer.calculate_compression_ratios()

    # 2. Rank by size range
    analyzer.rank_by_size_ranges()

    # 3. Create visualizations
    analyzer.create_visualizations()

    # 4. Create the predictor
    analyzer.create_predictor()

    results_dir = os.path.join(SCRIPT_DIR, 'results')
    print("\n" + "=" * 80)
    print("ANALYSIS COMPLETE")
    print("=" * 80)
    print(f"\nAll results are available in: {results_dir}/")
    print("  - average_compression_ratios.csv : Average ratios by compressor")
    print("  - rankings_by_size.csv           : Ranking by size range")
    print("  - predictor_rules.json           : Predictor rules")
    print("  - predictor.py                   : Usable predictor script")
    print("  - *.png                          : Graphical visualizations")
    print()


if __name__ == "__main__":
    main()
