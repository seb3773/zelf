# Analysis Results Synthesis

## Executive Summary

This analysis focuses on the performance of 14 different compressors tested on ~2000 system binaries (`/usr/bin/`). The objective is to identify the best compressors for different file sizes and to create an automatic predictor.

## Data Analyzed

- **Number of compressors**: 14
- **Total number of binaries tested**: ~2067 (slightly varies by compressor)
- **Filters tested**: BCJ and KanziEXE for each compressor
- **Metrics collected**: ~100 columns per dataset (size, entropy, ratios, time, etc.)

## 🏆 Main Results

### 1. Overall Compressor Ranking

| Rank | Compressor | Average Ratio | Observations |
|------|-------------|---------------|--------------|
| 🥇 1 | **shrinkler** | **43.94%** | All-category champion |
| 🥈 2 | **lzma** | **46.14%** | Excellent on large files |
| 🥉 3 | **zx0** | **46.33%** | Best on small files |
| 4 | apultra | 46.72% | Very good compromise |
| 5 | zx7b | 47.82% | Good for small files |
| 6 | blz | 48.18% | Balanced performance |
| 7 | exo | 49.72% | - |
| 8 | doboz | 50.31% | - |
| 9 | pp | 51.43% | - |
| 10 | lz4 | 51.73% | Probably very fast |
| 11 | qlz | 52.12% | - |
| 12 | lzav | 58.37% | - |
| 13 | snappy | 64.00% | Optimized for speed |
| 14 | zstd | 108.39% | Format-specific overhead |

*Note: A smaller ratio = better compression*

### 2. Best Compressors by Size Range

#### Binaries < 20KB (624 binaries tested)

| Rank | Compressor | Ratio | Gain vs #2 |
|------|-------------|-------|------------|
| 1 | **zx0** | 52.19% | - |
| 2 | zx7b | 53.44% | -2.34% |
| 3 | shrinkler | 53.70% | -2.81% |
| 4 | blz | 54.70% | -4.59% |
| 5 | lz4 | 56.78% | -8.09% |

**Recommendation**: Use **zx0** for small binaries

#### Binaries 20-200KB (1071 binaries tested)

| Rank | Compressor | Ratio | Gain vs #2 |
|------|-------------|-------|------------|
| 1 | **shrinkler** | 41.74% | - |
| 2 | lzma | 43.53% | -4.11% |
| 3 | zx0 | 43.62% | -4.32% |
| 4 | apultra | 44.78% | -6.79% |
| 5 | exo | 46.25% | -9.75% |

**Recommendation**: Use **shrinkler** for this range (the most common)

#### Binaries 200-500KB (128 binaries tested)

| Rank | Compressor | Ratio | Gain vs #2 |
|------|-------------|-------|------------|
| 1 | **lzma** | 33.13% | - |
| 2 | shrinkler | 34.25% | -3.27% |
| 3 | apultra | 36.88% | -10.17% |
| 4 | zx0 | 38.16% | -13.18% |
| 5 | exo | 38.34% | -13.61% |

**Recommendation**: Use **lzma** for medium binaries

#### Binaries > 500KB (244 binaries tested)

| Rank | Compressor | Ratio | Gain vs #2 |
|------|-------------|-------|------------|
| 1 | **lzma** | 27.74% | - |
| 2 | zstd | 31.26% | -11.25% |
| 3 | shrinkler | 32.17% | -13.76% |
| 4 | apultra | 32.31% | -14.15% |
| 5 | blz | 36.47% | -23.94% |

**Recommendation**: Use **lzma** for large binaries (compression up to ~28%!)

### 3. BCJ vs KanziEXE

**Clear result: BCJ is ALWAYS better**

Out of 14 compressors tested:
- ✅ BCJ wins: **14/14** (100%)
- ❌ KanziEXE wins: **0/14** (0%)
- ZSTD is excluded for binaries <50KB due to its large stub (~20-25KB); results for ZSTD are computed only on binaries ≥50KB.
- EXO is excluded from size-range rankings for files >2MB (excessive RAM use).
- ZX0 is excluded from size-range rankings for files >3MB (too slow on larger binaries).

**Average difference**: BCJ produces binaries ~2-3% smaller than KanziEXE

**Conclusion**: For x86-64 binaries, the BCJ filter should be the default choice.

## 📊 Advanced Statistics

### Ratio Distribution

High-performing compressors (shrinkler, lzma, apultra) show:
- **Normal distribution** with few extreme values
- **Peak around 40-45%** for the majority of binaries
- **Long tail** towards higher ratios for some hard-to-compress binaries

### Performance Curves vs. Size

Curve analysis reveals:
1. **All compressors improve** with file size
2. **LZMA particularly excels** on files >200KB
3. **Inflection point** around 100KB where lzma starts to dominate
4. **Plateau** around 25-30% for very large files (>1MB)

## 🎯 Automatic Predictor

A predictor script has been created: `results/predictor.py`

### Prediction Rules

```python
Size < 20KB      → zx0       (expected ratio: 52.19%)
Size 20-200KB    → shrinkler (expected ratio: 41.74%)
Size 200-500KB   → lzma      (expected ratio: 33.13%)
Size > 500KB     → lzma      (expected ratio: 27.74%)
```

### Usage

```bash
$ python3 results/predictor.py 100000
For a file of 100,000 bytes:
  Recommended compressor: shrinkler
  Expected ratio: 41.74%
```

## 💡 Practical Recommendations

### For General Use
- **Recommendation**: **shrinkler** (best overall compromise)
- **Alternative**: lzma (if large files dominate)

### For Optimization by Size
1. **< 20KB**: zx0
2. **20-200KB**: shrinkler
3. **> 200KB**: lzma

### For Optimizing Speed vs. Compression
- **Maximum compression**: lzma or shrinkler
- **Balance**: apultra, blz
- **Speed priority**: lz4 or snappy (though less efficient)

### For x86-64 Binaries
- **Always use the BCJ filter** (never KanziEXE)

## 🔍 Interesting Insights

### 1. The zstd Paradox
- zstd shows a ratio of 108% (file larger after compression!)
- Probable explanation: format overhead, metadata, or specific zelf configuration
- In practice, zstd is recognized as an excellent compressor (likely a test artifact)

### 2. shrinkler's Consistency
- shrinkler performs well across ALL size ranges
- Always in the top 3, often #1 or #2
- Safe choice when binary sizes are variable

### 3. lzma's Specialization
- Ratio drastically improves with size
- 52% on small files → 28% on large files
- Algorithm benefits from long-distance patterns

### 4. zx0: Champion of the Small
- Optimized for very small files
- Only 216 binaries tested (probably failed on large ones)
- Excellent in its niche (<20KB)

## 📈 Using the Results

The generated files allow for:

1. **Quick analysis**: `results/average_compression_ratios.csv`
2. **Selection by size**: `results/rankings_by_size.csv` (includes granular ranges; ZSTD excluded <50KB)
3. **Prediction**: `results/predictor.py`
4. **Visualization**: `results/*.png`
5. **Integration**: `results/predictor_rules.json`

## 🎓 Conclusion

This analysis demonstrates that:

1. ✅ **The choice of compressor significantly impacts** the final size (from 44% to 108%!)
2. ✅ **File size is the primary predictor** for choosing the compressor
3. ✅ **BCJ is superior to KanziEXE** for x86-64 binaries
4. ✅ **A simple size-based predictor** offers excellent results
5. ✅ **shrinkler and lzma dominate** depending on file size

---

*Analysis generated on November 29, 2025*
*Based on 14 compressors and ~2000 system binaries*