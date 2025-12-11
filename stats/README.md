# Binary Compression Datasets Analysis

This project analyzes the performance of different compressors used by the "zelf" executable packer on system binaries from `/usr/bin/`.

## Overview

The datasets were collected by testing 14 different compressors on over 2000 system binaries. For each compressor, two filters were tested:
- **BCJ** (Branch-Call-Jump) - filter optimized for x86 binaries
- **KanziEXE** - alternative filter for binaries

## Analyzed Compressors

The 14 tested compressors are:
- **shrinkler** - Best overall ratio (43.94%)
- **lzma** - Excellent for large files (>200KB)
- **zx0** - Optimal for small files (<20KB)
- **apultra**
- **zx7b**
- **blz**
- **exo**
- **doboz**
- **pp**
- **lz4**
- **qlz**
- **lzav**
- **snappy**
- **zstd** - Higher ratio but likely faster

## Project Structure

```
.
├── results/               # Analysis results
│   ├── average_compression_ratios.csv
│   ├── rankings_by_size.csv
│   ├── predictor_rules.json
│   ├── predictor.py       # Standalone predictor script
│   └── *.png             # Graphical visualizations
├── analyze_compressors.py # Main analysis script
└── README.md             # This file

Note: Datasets are read directly from ../build/predictor_datasets/
      ZSTD is excluded for binaries <50KB (stub size ~20-25KB).
```

## Usage

### 1. Run the full analysis

```bash
python3 analyze_compressors.py
```

This command will:
- Load all CSV datasets
- Calculate average compression ratios for each compressor
- Rank compressors by size range
- Generate visualizations (graphs, histograms, curves)
- Create a predictor to choose the best compressor

### 2. Use the predictor

```bash
# Recommendation for a 100KB file
python3 results/predictor.py 100000

# Recommendation for a 1MB file
python3 results/predictor.py 1000000
```

Example output:
```
For a file of 100,000 bytes:
  Recommended compressor: shrinkler
  Expected ratio: 41.74%
```

## Key Results

### Average Compression Ratios

| Compressor  | Average Ratio | Best Filter |
|-------------|---------------|-------------|
| shrinkler   | 43.94%        | BCJ         |
| lzma        | 46.14%        | BCJ         |
| zx0         | 46.33%        | BCJ         |
| apultra     | 46.72%        | BCJ         |
| zx7b        | 47.82%        | BCJ         |

### Recommendations by File Size

| Size          | Best Compressor | Expected Ratio |
|---------------|-----------------|----------------|
| < 20KB        | **zx0**         | 52.19%         |
| 20-200KB      | **shrinkler**   | 41.74%         |
| 200-500KB     | **lzma**        | 33.13%         |
| > 500KB       | **lzma**        | 27.74%         |

### Important Observations

1.  **BCJ is generally better**: For all tested compressors, the BCJ filter yields better ratios than KanziEXE.

2.  **LZMA dominates large files**: For binaries >200KB, LZMA offers the best compression ratios.

3.  **Shrinkler is the overall champion**: With an average ratio of 43.94%, shrinkler offers the best compromise across all sizes.

4.  **zstd has a high ratio** (108%): This might seem counter-intuitive, but it could be due to metadata or the specific format used by zelf.

## Available Visualizations

All visualizations are available in the `results/` folder:

1.  **average_ratios.png** - Bar chart of average ratios per compressor
2.  **performance_by_size_range.png** - Top 10 compressors for 4 broad ranges
3.  **performance_by_size_range_up_to_200K.png** - <15KB, 15-50KB, 50-100KB, 100-200KB
4.  **performance_by_size_range_up_to_500K.png** - 200-300KB, 300-400KB, 400-450KB, 450-500KB
5.  **performance_by_size_range_up_to_1500K.png** - 500-600KB, 600-800KB, 800KB-1MB, 1-1.5MB
6.  **performance_by_size_range_up_to_15000K.png** - 1.5-4MB, 4-7.5MB, 7.5-12MB, 12-15MB
7.  **performance_by_size_range_up_to_max.png** - 15-20MB, 20-40MB, 40-50MB, >50MB
8.  **bcj_vs_kanzi.png** - Direct comparison of BCJ vs KanziEXE filters
9.  **ratio_distributions.png** - Histograms of ratio distributions for each compressor
10. **performance_curves.png** - Performance curves based on binary size
11. **stub_overhead.png** - Stub sizes (BCJ vs KanziEXE, dynamic vs static) per compressor
12. **speed_speed_vs_ratio.png** - Compression speed vs ratio (bubble = sqrt(file size))
13. **speed_decomp_vs_ratio.png** - Decompression speed vs ratio (bubble = sqrt(file size))

## Reference Files

-   **collect_codec_deep.sh**: Bash script used to collect datasets (reference only)
-   **elfz_probe.c**: Source code for the tool that detects if a file is a valid ELF binary
-   **filtered_probe.c**: Source code for the tool that measures compression metrics

These files are provided for reference to understand how the data was collected.

## Dependencies

The analysis requires the following Python packages:
```bash
pip install pandas matplotlib seaborn numpy
```

## Dataset Location

**Important**: Datasets are read directly from `build/predictor_datasets/` (no duplication in `stats/datasets/`).

Exclusions applied in size-range rankings/plots:
- ZSTD excluded for binaries < 50KB (stub overhead).
- EXO excluded for binaries > 2MB (RAM use).
- ZX0 excluded for binaries > 3MB (very slow on large files).

The datasets should be generated using:
```bash
./tools/collect_codec_deep.sh --codec <codec> --out build/predictor_datasets/<codec>_deep.csv
```

## Collected Metrics

Each dataset contains approximately 100 columns of metrics, including:
-   File size
-   ELF type
-   Entropy (raw, BCJ, Kanzi)
-   Compression ratios
-   Final sizes after compression
-   Execution time
-   Memory usage
-   Winning filter (BCJ or KanziEXE)

## Contribution

This project was created to analyze the performance of compressors used by zelf. The datasets can be used to:
-   Develop better ML predictors
-   Optimize compressor choices
-   Compare the performance of new compressors
-   Analyze the impact of filters on compression

## License

The datasets and analysis scripts are provided for analysis and research purposes.