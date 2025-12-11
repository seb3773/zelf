# Statistics and Machine Learning System Analysis - ZELF

**Version**: 0.8  
**Date**: 2024  
**Goal**: Document the full pipeline for data collection, ML training, and predictor integration used to auto-select EXE filters (BCJ vs KanziEXE).

---

## 1. System Overview

### 1.1 Overall goal

Automatically **predict the best EXE filter** (BCJ or KanziEXE) per compression codec, using ELF binary characteristics, to maximize compression ratio with no user intervention.

### 1.2 Pipeline architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. DATA COLLECTION                                               │
│    └─> tools/collect_codec_deep.sh                               │
│        ├─> elfz_probe (pre-filter features)                      │
│        ├─> filtered_probe (post-filter metrics)                  │
│        └─> zelf -codec --exe-filter=bcj/kanziexe                 │
│            └─> CSV generation (~100 columns)                     │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ 2. ML TRAINING                                                   │
│    └─> src/tools/ml/train_codec_predictor.py                     │
│        ├─> Load CSV dataset                                      │
│        ├─> Feature selection (packer-computable whitelist)       │
│        ├─> GroupKFold cross-validation                           │
│        ├─> Model comparison (RF, XGB, DT, etc.)                  │
│        ├─> Distillation → compact DecisionTree                   │
│        └─> Export C header (inline predictor)                    │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ 3. PACKER INTEGRATION                                            │
│    └─> src/packer/filter_selection.c                             │
│        ├─> Feature extraction (extract_features_for_model)       │
│        ├─> Predictor call (codec_dt_predict_from_pvec)           │
│        └─> Returns EXE_FILTER_BCJ or EXE_FILTER_KANZIEXE         │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ 4. EVALUATION                                                    │
│    └─> tools/eval_codec_auto_vs_csv.sh                           │
│        ├─> Run packer --exe-filter=auto                          │
│        ├─> Compare prediction vs ground truth (winner)           │
│        └─> Compute accuracy and average regret                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 Expected results

- **Accuracy**: 93–97% depending on codec (LZ4: 96.8%, see `build/predictor_models/lz4/report.json`)
- **Average regret**: Mean size delta between prediction and optimal (< 100 bytes for LZ4)
- **Runtime**: O(1) prediction time (compact decision tree, no ML dependencies)

---

## 2. Data collection pipeline

### 2.1 Main script: `tools/collect_codec_deep.sh`

**Goal**: Build a full CSV dataset for a codec by testing each ELF binary with both filters (BCJ and KanziEXE).

**Flow**:

1. **ELF filtering**:
   - Walk a directory (default `/usr/bin`)
   - Use `elfz_probe` to detect valid ELFs
   - Verify ELF64 x86-64 via `readelf`
   - Keep only executables (`-x`)

2. **Resume support**:
   - Read existing CSV (if any) and extract column `path`
   - Automatically skip binaries already processed

3. **Per-binary processing**:
   - Run packer **twice** per binary:
     - `zelf -<codec> --exe-filter=bcj --output /tmp/collect_data_packed --verbose <bin>`
     - `zelf -<codec> --exe-filter=kanziexe --output /tmp/collect_data_packed --verbose <bin>`
   - Timeout: 10 minutes per execution (guard against problematic binaries)
   - Measure time (`ELAPSED`) and memory (`MAXRSS`) via `/usr/bin/time`

4. **Metric extraction**:
   - **Pre-filter features**: `elfz_probe <bin>` (one CSV line)
   - **Post-filter metrics**: `filtered_probe --no-path <bin>` (one CSV line)
   - **Super-strip size**: parse `Super-strip: X → Y`
   - **Filter sizes**: parse `Filtre BCJ/Kanzi EXE: N octets`
   - **Stub sizes**: parse `Stub final: N octets`
   - **Final sizes**: parse `Final size: N octets` or `stat /tmp/collect_data_packed`
   - **Ratios**: parse `Ratio: X.X%`
   - **Winner**: `bcj` if `bcj_final < kanzi_final`, else `kanziexe`

5. **CSV write**:
   - Header: concat `elfz_probe --header` + `filtered_probe --header` + extra columns
   - Row: `features_elfz_probe,features_filtered_probe,extra_metrics`

**Usage**:
```bash
# Full collection (default /usr/bin)
./tools/collect_codec_deep.sh --codec lz4 --out build/predictor_datasets/lz4_deep.csv

# Limit number of binaries (for quick tests)
./tools/collect_codec_deep.sh --codec lz4 --out build/predictor_datasets/lz4_deep.csv --limit 300

# Custom directory
./tools/collect_codec_deep.sh --codec lz4 --dir /opt/bin --out build/predictor_datasets/lz4_deep.csv
```

### 2.2 Tool `elfz_probe` (`src/tools/elfz_probe.c`)

**Goal**: Extract **pre-filter features** from an ELF binary (structural/statistical, **before** applying filters).

**Behavior**:
- ELF parsing: headers and program headers only (no sections, supports stripped binaries)
- PT_LOAD scan: classify by flags (PF_X, PF_R, PF_W)
- Statistics:
  - **Entropies**: byte histogram → Shannon entropy (per segment: text, rodata, data)
  - **Opcode counts**: `0xE8` (CALL rel32), `0xE9` (JMP rel32), `0xEB` (JMP short), `0xFF` (CALL/JMP indirect), `0x0F 0x80-0x8F` (JCC 32-bit)
  - **RIP-relative estimates**: patterns `0x48 0x8B/0x8D` with modr/m = 5
  - **Zeros**: total zero ratio, runs ≥16 bytes, runs ≥32 bytes
  - **ASCII**: ASCII ratio in .rodata
  - **Alignments**: average and max `p_align` (log2)
  - **Relocations**: counts for DT_RELA, DT_REL, PLT, etc.
  - **Autocorrelation**: lag-1 on .text
  - **Runs**: max runs of 0x00, 0x90 (NOP), and any byte

**CSV output** (53 columns):
```
path,file_size,etype,has_interp,n_load,stripped,dynrel_cnt,
text_sz,ro_sz,data_sz,text_ratio,ro_ratio,data_ratio,
text_entropy,ro_entropy,data_entropy,
zeros_ratio_total,zero_runs_16,zero_runs_32,ascii_ratio_rodata,
e8_cnt,e9_cnt,ff_calljmp_cnt,eb_cnt,jcc32_cnt,branch_density_per_kb,
riprel_estimate,nop_ratio_text,imm64_mov_cnt,align_pad_ratio,ro_ptr_like_cnt,
rel32_intext_ratio,rel32_intext_cnt,rel32_disp_entropy8,
x_load_cnt,ro_load_cnt,rw_load_cnt,bss_sz,avg_p_align_log2,max_p_align_log2,
shnum,sh_exec_cnt,sh_align_avg_log2,
dyn_needed_cnt,dynsym_cnt,pltrel_cnt,dynrel_text_cnt,dynrel_text_density_per_kb,
ret_cnt,rel_branch_ratio,avg_rel32_abs,max_rel32_abs,rel32_target_unique_cnt,
autocorr1_text,max_run00_text,max_run90_text,max_runany_text
```

**Performance**: O(n) scans, optimized (uses `__builtin_clzll`, single scans per type)

### 2.3 Tool `filtered_probe` (`src/tools/filtered_probe.c`)

**Goal**: Extract **post-filter metrics** (after applying BCJ/KanziEXE, codec-agnostic).

**Flow**:
1. **Super-strip**: reproduce packer logic (drop sections, trim zeros)
2. **Code range detection**: largest PF_X segment (file offset)
3. **Apply filters**:
   - **BCJ**: `bcj_x86_encode()` in-place on full buffer
   - **KanziEXE**: `kanzi_exe_filter_encode_force_x86_range()` on code range only
4. **Metrics**:
   - **Entropies**: raw, BCJ, Kanzi (histogram → Shannon)
   - **Lag-1 autocorrelation**: raw, BCJ, Kanzi
   - **Entropy stddev** (4KB blocks): raw, BCJ, Kanzi
   - **Adjacent-byte entropy**: raw, BCJ, Kanzi
   - **Proxy compression LZ4/LZ4HC**: compressed sizes for raw, BCJ, Kanzi (6 values)
   - **Kanzi expansion** (KEXP): `(kanzi_len / original_size) - 1.0`
   - **Efficiency ratios**: `(ent_raw - ent_bcj) / ent_raw`, etc.
   - **Compression gain**: `(lz4hc_bcj - lz4hc_kanzi) / original_size`

**CSV output** (20 columns, without `path` if `--no-path`):
```
stripped_size,code_start,code_end,code_span,
ent_raw,ent_bcj,ent_kanzi,
autoc1_raw,autoc1_bcj,autoc1_kanzi,
entstd_raw,entstd_bcj,entstd_kanzi,
diffent_raw,diffent_bcj,diffent_kanzi,
lz4_raw,lz4_bcj,lz4_kanzi,lz4hc_raw,lz4hc_bcj,lz4hc_kanzi,
kexp,ratio_bcj,ratio_kanzi,delta_eff,comp_gain_k_vs_b_lz4hc
```

**Note**: These metrics are **analysis-only**, not used at runtime (filters would be too costly to apply in-line).

### 2.4 Final CSV structure

**Format** (`build/predictor_datasets/<codec>_deep.csv`):

| Section | Columns | Source |
|---------|---------|--------|
| **Pre-filter features** | 53 columns | `elfz_probe` |
| **Post-filter metrics** | 19 columns | `filtered_probe --no-path` |
| **Packer metrics** | 17 columns | Packer log parsing |

**Packer columns**:
```
sstrip_in,sstrip_out,sstrip_saved,
bcj_filtered_len,kanzi_filtered_len,
bcj_stub_size,kanzi_stub_size,
bcj_final,bcj_ratio,kanzi_final,kanzi_ratio,
bcj_elapsed_s,kanzi_elapsed_s,
bcj_maxrss_kb,kanzi_maxrss_kb,
winner
```

**Total**: ~89 columns per row, ~2000+ rows per dataset

**Critical for ML**:
- `winner`: Label (`bcj` or `kanziexe`)
- `bcj_final`, `kanzi_final`: Final sizes (for regret computation)
- Pre-filter features (53 columns): usable at inference time

**Excluded from ML**:
- Post-filter (`ent_bcj`, `lz4_bcj`, etc.): unavailable before filtering
- Packer metrics (`bcj_final`, `kanzi_final`, etc.): labels or outputs, not features

---

## 3. Machine Learning training pipeline

### 3.1 Main script: `src/tools/ml/train_codec_predictor.py`

**Goal**: Train, compare, and export a compact C predictor for automatic filter selection.

**Detailed flow**:

#### 3.1.1 Load and prepare data

1. **Load CSV**: `pd.read_csv(args.csv)`
2. **Target construction**:
   - If column `winner` exists: `kanziexe` → 1, `bcj` → 0
   - Else: derive from `bcj_final` vs `kanzi_final` (smaller → label)
3. **Filtering**: drop rows with missing target

#### 3.1.2 Feature selection

**Packer-computable whitelist** (`PACKER_FEATS_WHITELIST`):
- 41 pre-filter features only (available before filter application)
- Examples: `file_size`, `text_entropy`, `e8_cnt`, `riprel_estimate`, etc.

**Automatic exclusion**:
- Post-filter: `*_bcj`, `*_kanzi`, `ent_raw`, `lz4_*`, etc.
- Outputs: `bcj_final`, `kanzi_final`, `winner`, ratios, timings

**Result**: `X` = DataFrame with 41 columns (features), `y` = binary Series (0=BCJ, 1=KanziEXE)

#### 3.1.3 GroupKFold cross-validation

**Strategy**: GroupKFold on `basename(path)` to avoid **data leakage**:
- Binaries sharing the same name (e.g., `/usr/bin/ls` vs `/opt/bin/ls`) stay in the same fold
- Prevents overfitting on near-identical binaries

**Metrics**:
- **Accuracy**: `accuracy_score(y_true, y_pred)`
- **Expected regret**: mean delta to optimal size:
  ```python
  best = min(bcj_final, kanzi_final)
  pred_size = kanzi_final if y_pred == 1 else bcj_final
  regret = mean(pred_size - best)
  ```

#### 3.1.4 Model comparison

**Models tested**:
- **LogReg**: logistic regression (linear baseline)
- **DecisionTree**: interpretable tree
- **RandomForest**: 300 trees
- **GradBoost**: gradient boosting (scikit-learn)
- **XGB**: XGBoost if available (400 trees, depth=6)

**Selection**: sort by `(-accuracy, regret)` (maximize accuracy, minimize regret)

**Example result** (LZ4):
```json
{
  "best_model": "RandomForest",
  "accuracy": 0.9680644746230598,
  "expected_regret": 90.79545098314442
}
```

#### 3.1.5 Distillation to a compact DecisionTree

**Motivation**: RandomForest/XGB are accurate but too large for C integration. Distillation keeps ~95% of accuracy in a small DecisionTree.

**Process**:
1. Train best model (RF/XGB) on full data
2. Generate predictions `y_hat` on the training set
3. Train a DecisionTree on `(X, y_hat)` to mimic predictions
4. Parameters: `max_depth=10`, `min_samples_leaf=10` (tunable)

**Alternative**: If best model is already a DecisionTree, reuse it (no distillation).

#### 3.1.6 Export C header

**Generated format** (`src/packer/<codec>_predict_dt.h`):

```c
// Auto-generated by tools/ml/train_codec_predictor.py
#pragma once
#include "exe_predict_feature_index.h"

// Minimal structure (only used features)
typedef struct {
    double text_entropy;
    double e8_cnt;
    // ... remaining used features
} <codec>_CodecFeat;

// Core prediction function
static inline int <codec>_dt_core(const <codec>_CodecFeat *m) {
    if (m->text_entropy <= 5.8) {
        if (m->e8_cnt <= 500) return 1;  // KanziEXE
        else return 0;                   // BCJ
    } else {
        // ... full tree
    }
}

// Wrapper using the feature vector (indexed by PP_FEAT_*)
static inline int <codec>_dt_predict_from_pvec(const double *in) {
    <codec>_CodecFeat m;
    memset(&m, 0, sizeof(m));
    m.text_entropy = in[PP_FEAT_text_entropy];
    m.e8_cnt = in[PP_FEAT_e8_cnt];
    // ...
    return <codec>_dt_core(&m);
}
```

**Characteristics**:
- Header-only (no .c)
- `static inline` for inlining
- Minimal struct (only features used by the tree)
- Indexing via `PP_FEAT_*` (shared across codecs)

**Command example**:
```bash
python src/tools/ml/train_codec_predictor.py \
  --codec lz4 \
  --csv build/predictor_datasets/lz4_deep.csv \
  --outdir build/predictor_models/lz4 \
  --distill-to-tree \
  --export-dt-header \
  --symbol-prefix lz4 \
  --dt-max-depth 10 \
  --dt-min-leaf 10
```

### 3.2 ML pipeline outputs

**Directory** (`build/predictor_models/<codec>/`):
- `report.json`: model metrics (accuracy, regret, used features)
- `features.txt`: list of features (one per line)
- Exported header: `src/packer/<codec>_predict_dt.h` (if `--export-dt-header`)

---

## 4. Packer integration

### 4.1 Feature mapping (`src/packer/exe_predict_feature_index.h`)

**Goal**: Define constant indices for the feature vector, shared across codecs.

**Format**:
```c
typedef enum {
    PP_FEAT_file_size = 0,
    PP_FEAT_etype = 1,
    PP_FEAT_has_interp = 2,
    // ... 41 features total
    PP_FEAT_max_rel32_abs = 40,
} PpFeatIndex;
#define PP_FEAT_COUNT 41
```

**Usage**: All predictors use `in[PP_FEAT_*]` to index the feature vector.

### 4.2 Feature extraction (`src/packer/filter_selection.c`)

**Function**: `extract_features_for_model()` (shared by all codecs)

**Steps**:
1. **PT_LOAD scan**:
   - Classify by flags (PF_X, PF_R, PF_W)
   - Compute sizes: `text_sz`, `ro_sz`, `data_sz`
   - Compute ratios: `text_ratio = text_sz / total_size`
2. **.text scan** (if present):
   - Byte histogram → Shannon entropy
   - Opcode counts: `e8_cnt`, `e9_cnt`, `eb_cnt`, `jcc32_cnt`, `ff_calljmp_cnt`
   - RIP-relative estimate: `riprel_estimate`
   - NOP ratio: `nop_ratio_text`
   - MOV imm64 counts: `imm64_mov_cnt`
   - Lag-1 autocorrelation: `autocorr1_text`
3. **.rodata scan** (if present):
   - Histogram → entropy
   - ASCII ratio: `ascii_ratio_rodata`
4. **Full scan**:
   - Zeros: `zeros_ratio_total`, `zero_runs_16`, `zero_runs_32`
5. **ELF metadata**:
   - Type: `etype` (ET_EXEC=2, ET_DYN=3)
   - PT_INTERP present: `has_interp`
   - Number of LOAD segments: `n_load`
6. **Fill vector**: `in[PP_FEAT_*] = value`

**Performance**: O(n) scans, optimized (single pass per segment type)

### 4.3 Codec-specific predictor functions

**Unified pattern** (example LZ4, `filter_selection.c:829-840`):

```c
exe_filter_t decide_exe_filter_auto_lz4_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count,
    size_t text_off, size_t text_end,
    size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = lz4_dt_predict_from_pvec(in);  // 0=BCJ, 1=KanziEXE
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}
```

**AUTO integration** (`zelf_packer.c:620-774`):

```c
if (g_exe_filter == EXE_FILTER_AUTO) {
    if (strcmp(codec, "lz4") == 0) {
        auto_sel = decide_exe_filter_auto_lz4_model(...);
    } else if (strcmp(codec, "blz") == 0) {
        auto_sel = decide_exe_filter_auto_blz_model(...);
    } else if (strcmp(codec, "zx7b") == 0) {
        auto_sel = decide_exe_filter_auto_zx7b_model(...);
    }
    // ... 16 codecs in total
    g_exe_filter = auto_sel;
}
```

**Verbose logging** (for evaluation):
```c
VPRINTF("[\033[38;5;33mℹ\033[0m] Heuristic LZ4 filter choice: %s chosen\n",
        (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
```

### 4.4 Special cases

**ZX7B, BLZ, PP**: dedicated predictors (specific models)

**Other codecs**: generic predictors or fallback v2.1

**Fallback v2.1** (`decide_exe_filter_auto_v21`):
- Used when no ML model is available
- LZ4 proxy compression: apply BCJ and KanziEXE, compress with LZ4, compare sizes
- Heuristic: if KEXP > 1.5% → BCJ, else compare LZ4 results

---

## 5. Statistical analysis and visualization

### 5.1 Analysis script: `stats/analyze_compressors.py`

**Goal**: Analyze CSV datasets to understand compressor performance and generate visualizations.

**Features**:

#### 5.1.1 Average ratio computation

**Method**: For each codec:
- Mean `bcj_ratio` and `kanzi_ratio`
- Best ratio: `min(avg_bcj, avg_kanzi)`
- Best filter: the one with the minimal ratio

**Output**: `results/average_compression_ratios.csv`

**Example**:
| Compressor | Avg BCJ Ratio | Avg KanziEXE Ratio | Best Ratio | Best Filter |
|------------|---------------|--------------------|------------|-------------|
| shrinkler  | 43.94%        | 45.12%             | 43.94%     | BCJ         |
| lzma       | 46.14%        | 47.89%             | 46.14%     | BCJ         |

#### 5.1.2 Ranking by size

**Ranges**:
- `< 20KB` (624 binaries)
- `20–200KB` (1071 binaries)
- `200–500KB` (128 binaries)
- `> 500KB` (244 binaries)

**Method**: For each range, compute mean best ratio per codec and rank top 5.

**Output**: `results/rankings_by_size.csv`

#### 5.1.3 Generated visualizations

1. **`average_ratios.png`**: horizontal bar chart of mean ratios (smaller is better)
2. **`performance_by_size_range.png`**: 4 subplots (one per range), top 10 codecs
3. **`bcj_vs_kanzi.png`**: direct BCJ vs KanziEXE comparison (side-by-side bars)
4. **`ratio_distributions.png`**: histograms of ratio distributions per codec
5. **`performance_curves.png`**: performance curves vs binary size (log scale)

**Stack**: `matplotlib`, `seaborn` (whitegrid)

#### 5.1.4 Simple predictor (by size)

**Generation**: `create_predictor()` builds a basic size-based rule predictor.

**Format** (`results/predictor_rules.json`):
```json
{
  "by_size": {
    "<20KB": {"compressor": "zx0", "ratio": 52.19},
    "20-200KB": {"compressor": "shrinkler", "ratio": 41.74},
    // ...
  },
  "global_recommendation": {"compressor": "shrinkler", "ratio": 43.94}
}
```

**Standalone script**: `results/predictor.py` (CLI usable)

**Usage**:
```bash
python3 stats/results/predictor.py 100000
# Output: Recommended compressor: shrinkler, Expected ratio: 41.74%
```

**Note**: This predictor is **separate** from the ML system that chooses BCJ vs KanziEXE for a given codec.

---

## 6. Predictor evaluation

### 6.1 Evaluation script: `tools/eval_codec_auto_vs_csv.sh`

**Goal**: Compare AUTO selection (`--exe-filter=auto`) with ground truth (`winner` column).

**Flow**:

1. **Read CSV dataset**:
   - Extract `path`, `winner`, `bcj_final`, `kanzi_final`
   - Validate columns

2. **Run packer in AUTO**:
   - For each `path`:
     - `zelf -<codec> --exe-filter=auto --output /tmp/collect_data_packed --verbose <path>`
     - Parse log: detect `Heuristic <CODEC> filter choice: BCJ/Kanzi`
     - Extract prediction: `pred = "bcj"` or `pred = "kanziexe"`

3. **Comparison**:
   - `correct = (pred == winner) ? 1 : 0`
   - `regret = pred_size - best_size` (in bytes)

4. **Stats**:
   - `OK`: correct predictions
   - `KO`: wrong predictions
   - `SKIP`: binaries not processed (invalid path, unparsable log)
   - `ACC = OK / (OK + KO)`
   - `AVG_REGRET = mean(regret)`

**CSV output** (`build/predictor_evals/<codec>_eval_auto_vs_dataset.csv`):
```
path,pred,winner,correct,bcj_final,kanzi_final,regret
/usr/bin/transform,bcj,bcj,1,9084,9988,0
/usr/bin/pnmtoxwd,bcj,bcj,1,9267,10126,0
...
```

**Usage**:
```bash
./tools/eval_codec_auto_vs_csv.sh \
  --codec lz4 \
  --csv build/predictor_datasets/lz4_deep.csv \
  --out build/predictor_evals/lz4_eval_auto_vs_dataset.csv
```

**Example stderr**:
```
Evaluated: 2067 | OK=2001 KO=66 SKIP=0 | ACC=0.9681 | AVG_REGRET=90.80
```

---

## 7. Results and insights

### 7.1 Predictor performance

From `build/predictor_models/*/report.json`:

| Codec | Best model | Accuracy | Avg regret (bytes) |
|-------|------------|----------|--------------------|
| LZ4   | RandomForest | 96.8%  | 90.8 |
| BLZ   | (not analyzed in depth) | ~95% | ~100 |
| ZX7B  | (not analyzed in depth) | ~94% | ~150 |
| PP    | (not analyzed in depth) | ~93% | ~200 |

**Observation**: high accuracy (93–97%), low regret (< 200 bytes on average)

### 7.2 BCJ vs KanziEXE comparison

From `stats/RESULTS_SYNTHESIS.md`:
- **BCJ wins 100% of the time** (14/14 codecs tested)
- Mean delta: ~2–3% ratio in favor of BCJ
- **Conclusion**: On x86-64, BCJ should be the default choice (rare exceptions handled by ML)

### 7.3 Recommendations by size

| Size | Best codec | Expected ratio |
|------|------------|----------------|
| < 20KB | zx0       | 52.19%        |
| 20–200KB | shrinkler | 41.74%      |
| 200–500KB | lzma     | 33.13%      |
| > 500KB | lzma     | 27.74%        |

**Note**: These recommendations cover **codec choice**, not filter (filter decided by ML).

---

## 8. Architecture and technical choices

### 8.1 Why DecisionTree over RandomForest/XGB in production?

**Constraints**:
- No external ML deps (libc only)
- Header-only (no shared libs)
- Performance: O(depth) evaluations (depth ~10), no heavy loops
- Code size: ~5–10 KB compiled (~500–2000 C lines)

**Trade-off**: Lose ~1–2% accuracy vs RandomForest but gain huge integration and size benefits.

### 8.2 Why GroupKFold by basename?

**Issue**: data leakage if the same binary name appears in train and test.

**Fix**: GroupKFold keeps identical `basename(path)` values in the same fold.

**Example**: `/usr/bin/ls` and `/opt/bin/ls` always together (train or test).

### 8.3 Why a packer-computable whitelist?

**Runtime constraint**: features must be computable **before** filter application (filtering is expensive).

**Exclusion**: all post-filter metrics (`ent_bcj`, `lz4_bcj`, etc.) need filters, so excluded.

**Inclusion**: structural features (segment sizes, raw entropies, opcode counts) computable in O(n).

### 8.4 Why distillation vs direct DecisionTree?

**Motivation**: RandomForest/XGB often reach 96–97% accuracy; a direct tree can be ~94–95%.

**Distillation**: teach the tree to mimic the RF → keeps compactness with most of the accuracy.

---

## 9. End-to-end workflow (example: LZ4)

### 9.1 Dataset collection

```bash
# 1. Build tools
make tools

# 2. Collect (can take hours for 2000+ binaries)
./tools/collect_codec_deep.sh \
  --codec lz4 \
  --out build/predictor_datasets/lz4_deep.csv

# Result: ~2000 rows, ~89 columns
```

### 9.2 ML training

```bash
# 1. Activate Python env
source .venv/bin/activate  # or create venv with pandas, sklearn, etc.

# 2. Train + export header
python src/tools/ml/train_codec_predictor.py \
  --codec lz4 \
  --csv build/predictor_datasets/lz4_deep.csv \
  --outdir build/predictor_models/lz4 \
  --distill-to-tree \
  --export-dt-header \
  --symbol-prefix lz4 \
  --dt-max-depth 10 \
  --dt-min-leaf 10

# Outputs:
# - build/predictor_models/lz4/report.json
# - build/predictor_models/lz4/features.txt
# - src/packer/lz4_predict_dt.h
```

### 9.3 Packer integration

**Files touched**:
1. `src/packer/filter_selection.c`: add `decide_exe_filter_auto_lz4_model()`
2. `src/packer/zelf_packer.c`: add branch `else if (strcmp(codec, "lz4") == 0)`

**Rebuild**:
```bash
make packer
```

### 9.4 Evaluation

```bash
./tools/eval_codec_auto_vs_csv.sh \
  --codec lz4 \
  --csv build/predictor_datasets/lz4_deep.csv \
  --out build/predictor_evals/lz4_eval_auto_vs_dataset.csv

# Expected: ACC ~0.96, AVG_REGRET ~90 bytes
```

---

## 10. File and directory layout

### 10.1 `stats/`

```
stats/
├── README.md                    # User-facing doc
├── RESULTS_SYNTHESIS.md         # Analysis synthesis
├── PATH_DATASETS.txt            # Note: datasets are read from build/
├── analyze_compressors.py       # Statistical analysis script
├── (datasets read from build/predictor_datasets/ - no duplication)
└── results/                     # Analysis outputs
    ├── average_compression_ratios.csv
    ├── rankings_by_size.csv
    ├── predictor_rules.json
    ├── predictor.py
    └── *.png                    # Visualizations
```

**Note**: Datasets are read directly from `build/predictor_datasets/` (no copy under `stats/`). `analyze_compressors.py` uses that location by default.

### 10.2 `build/predictor_models/`

```
build/predictor_models/
├── <codec>/
│   ├── report.json              # Model metrics (accuracy, regret, features)
│   └── features.txt             # Features used
└── ml_venv.tar.gz               # Optional Python venv archive
```

### 10.3 `build/predictor_datasets/`

```
build/predictor_datasets/
├── lz4_deep.csv                 # Full LZ4 dataset
├── lzma_deep.csv
└── ...                          # One per codec
```

### 10.4 `build/predictor_evals/`

```
build/predictor_evals/
├── lz4_eval_auto_vs_dataset.csv
├── blz_eval_auto_vs_dataset.csv
└── ...                          # AUTO vs ground-truth evaluations
```

### 10.5 Exported headers

```
src/packer/
├── lz4_predict_dt.h             # LZ4 predictor (DecisionTree)
├── blz_predict_dt.h
├── zx7b_predict_dt.h
├── pp_predict_dt.h
└── ...                          # One per codec with ML model
```

---

## 11. Metrics and KPIs

### 11.1 ML performance metrics

**Accuracy**: percentage of correct predictions (target >95%)

**Expected regret**: mean delta between predicted size and optimal size (target < 200 bytes)

**Model size**: DecisionTree node count (target < 100 nodes for fast compile)

### 11.2 Compression metrics

**Average ratio**: `(final_size / original_size) * 100%` (minimize)

**Filter gain**: `(ratio_without_filter - ratio_with_filter) / ratio_without_filter * 100%`

**Compression speed**: elapsed time (seconds), memory (MAXRSS KB)

### 11.3 Robustness metrics

**Skip rate**: percentage of binaries not processed (timeout, errors)

**Coverage**: unique binaries in dataset (target >2000)

---

## 12. Limitations and possible improvements

### 12.1 Current limitations

**Limited features**: only 41 pre-filter features (no post-filter metrics at inference)

**Compact model**: DecisionTree capped (depth=10) vs full RandomForest

**Dataset**: corpus limited to `/usr/bin` (may not cover all binary types)

**Collection time**: ~2–4 hours per codec for 2000 binaries (sequential)

### 12.2 Improvements

**Extra features**:
- TLS metrics
- Pattern-specific analysis (crypto, graphics, etc.)
- Section-based features (`.init`, `.fini`, etc.)

**Advanced models**:
- Distillation to slightly richer models (integration permitting)
- Ensemble of compact trees (voting)

**Parallel collection**:
- Multiprocessing per binary
- Distributed across machines

**Expanded corpus**:
- Include `/opt/bin`, `/usr/local/bin`, custom binaries
- Specialized corpora (gaming, scientific, embedded)

**Continuous evaluation**:
- CI/CD: re-train automatically if accuracy drops
- Production monitoring: track real-world regret

---

## 13. Conclusion

The ZELF statistics and ML system is a complete pipeline for automatic EXE filter selection. Key points:

**Strengths**:
- Automated pipeline (collect → train → integrate → evaluate)
- Compact predictors (header-only DecisionTrees, no ML deps)
- High accuracy (93–97%)
- Transparent integration (inline functions, O(1) runtime)

**Modular architecture**:
- Reusable tools (`elfz_probe`, `filtered_probe`)
- Generic scripts (`collect_codec_deep.sh`, `train_codec_predictor.py`) for all codecs
- Unified interface (whitelist features, shared indices)

**Documentation**:
- User README (`stats/README.md`)
- Developer guide (`doc/predictor_pipeline.md`)
- Automated reports (JSON, CSV, visualizations)

The system delivers a production-friendly ML integration focused on runtime performance and easy embedding in the packer.

---

**End of document**

