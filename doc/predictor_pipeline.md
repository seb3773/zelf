# Predictor pipeline (dataset → Python model → C header → integration → evaluation)

This document is the reference to reproduce the full pipeline for any supported codec:
- collect a labeled dataset (BCJ vs KanziEXE) for a codec
- train and distill a compact DecisionTree
- export a self-contained C header predictor
- integrate the predictor into the packer
- evaluate AUTO decision vs dataset

All paths are relative to the repository root.


## 0) Scope and constraints

- Target platform: Linux x86_64 (only ELF64 x86-64 binaries are considered)
- Do not modify the packer’s stub selection logic or PT_INTERP handling. Only integrate the predictor in the existing AUTO selection branch (this document covers only that).
- The exported C predictor must be header-only, fast (O(n) scans, O(1) memory), and integrated without external ML dependencies.


## 1) Prerequisites

- Build artifacts:
  - `make -j packer` → builds `build/zelf`
  - `make -j tools` → builds `build/tools/elfz_probe` and `build/tools/filtered_probe` (the collector will build them automatically if missing)
- Python environment:
  - Use a Python venv with `numpy`, `pandas`, `scikit-learn` installed.
  - If the repository provides one: `build/predictor_models/ml_venv/`.
  - Sanity checks (recommended before training):
    ```bash
    build/predictor_models/ml_venv/bin/python -c "import numpy,pandas,sklearn; print('numpy', numpy.__version__); print('pandas', pandas.__version__); print('sklearn', sklearn.__version__)"
    build/predictor_models/ml_venv/bin/python -m pip check
    ```
  - Version pinning note:
    - `scikit-learn<2` typically requires `numpy<2`. If `pip` upgrades `numpy` to `2.x`, reinstall `numpy==1.26.4` (or any `<2` compatible version).
- Supported codec names (examples):
  - `lz4, apultra, zx7b, zx0, blz, pp, zstd, lzma, qlz, snappy, doboz, lzav, exo, shrinkler`

Standard locations:
- Datasets: `build/predictor_datasets/<codec>_deep.csv`
- Models/reports: `build/predictor_models/<codec>/`
- Exported header: `src/packer/<codec>_predict_dt.h`
- Evaluations: `build/predictor_evals/<codec>_eval_auto_vs_dataset.csv`


## 2) Dataset collection

Script: `tools/collect_codec_deep.sh`

### Purpose
For a given codec, iterate over a directory of ELF executables (default `/usr/bin`), run the packer twice per file (once with `--exe-filter=bcj`, once with `--exe-filter=kanziexe`), and record:
- `elfz_probe` feature row
- `filtered_probe` row (post-filter metrics)
- super-strip sizes (in/out/saved)
- filtered lengths (BCJ and Kanzi), stub sizes
- final packed sizes and ratios per filter
- ELAPSED seconds and MAXRSS KB (per run)
- winner label (`bcj` or `kanziexe`) based on final packed sizes

### Usage
```bash
# EXO example (default dir /usr/bin)
./tools/collect_codec_deep.sh --codec exo --out build/predictor_datasets/exo_deep.csv

# Limit the number of newly processed binaries (resume supported)
./tools/collect_codec_deep.sh --codec exo --out build/predictor_datasets/exo_deep.csv --limit 300

# Different source directory
./tools/collect_codec_deep.sh --codec exo --dir /opt/bin --out build/predictor_datasets/exo_deep.csv
```

### Input filtering and resume
- Filters to ELF64 x86-64 executables using `readelf` and executable bit.
- Resume: if the CSV already exists, paths already present are skipped.
- Timeouts: each packer run uses `timeout 10m` if available.

### CSV schema (minimum required for evaluation)
`path,winner,bcj_final,kanzi_final` must be present.
The collector prepends many feature columns from `elfz_probe --header` and `filtered_probe --header --no-path`, then appends extra metrics; the evaluator only requires the above four columns.


## 3) Model training and distilled export

Script: `src/tools/ml/train_codec_predictor.py`

### Purpose
- Train multiple models on the dataset and pick the best
- Distill a compact DecisionTree with comparable accuracy
- Export a header-only predictor in C for inline inference

### Typical command
```bash
build/predictor_models/ml_venv/bin/python src/tools/ml/train_codec_predictor.py \
  --codec exo \
  --csv build/predictor_datasets/exo_deep.csv \
  --outdir build/predictor_models/exo \
  --distill-to-tree --export-dt-header \
  --symbol-prefix exo \
  --dt-max-depth 10 --dt-min-leaf 10
```

Key arguments:
- `--symbol-prefix exo`: ensures all exported symbols (struct typedef and functions) are prefixed to avoid collisions across codecs.
- `--distill-to-tree --export-dt-header`: triggers distilled DecisionTree export to `src/packer/<codec>_predict_dt.h`.
- `--dt-max-depth / --dt-min-leaf`: control model size vs accuracy/regret.

Outputs:
- `src/packer/<codec>_predict_dt.h`: header with:
  - a minimal feature struct (only required fields),
  - `static inline int <codec>_dt_predict_from_pvec(const double *in)` returning `0` (BCJ) or `1` (KANZIEXE).
- `build/predictor_models/<codec>/`: training logs and metrics.

Naming recommendation (if needed):
- Ensure the typedef is `<codec>_CodecFeat` so multiple headers can be included together cleanly.


## 4) Predictor integration in the packer

File: `src/packer/zelf_packer.c`

### Steps
1) Include the exported header near other predictors:
   ```c
   #include "exo_predict_dt.h"
   ```

   Include order note:
   - Ensure `<string.h>` is included before any `*_predict_dt.h` header (the generated predictors may call `memset`).

2) Implement the codec-specific auto selector that:
   - scans PT_LOAD segments and aggregates the same features used in other selectors (see existing functions as reference: `decide_exe_filter_auto_lz4_model`, `..._lzav_...`, `..._zstd_...`, etc.)
   - fills a feature vector indexed by `src/packer/exe_predict_feature_index.h` (`PP_FEAT_*` indices)
   - calls the predictor and returns an `exe_filter_t`

   Skeleton:
   ```c
   static exe_filter_t decide_exe_filter_auto_exo_model(
       const unsigned char *combined, size_t actual_size,
       const SegmentInfo *segments, size_t segment_count,
       size_t text_off, size_t text_end,
       size_t orig_size)
   {
       // 1) scan segments (TEXT/RO/DATA), build histograms and counters
       // 2) compute derived metrics (entropies, ratios, rel32 stats, etc.)
       // 3) fill in[PP_FEAT_COUNT] respecting PP_FEAT_* indices
       // 4) run predictor
       int pred = exo_dt_predict_from_pvec(in); // 1=KANZIEXE, 0=BCJ
       return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
   }
   ```

3) Wire it into AUTO selection:
   - In the AUTO branch (search `if (g_exe_filter == EXE_FILTER_AUTO)`), add the codec branch:
     ```c
     } else if (use_exo) {
         auto_sel = decide_exe_filter_auto_exo_model(combined, actual_size, segments, segment_count, text_off, text_end, size);
     ```

4) Add the verbose log consumed by the evaluator:
   ```c
   } else if (use_exo) {
       VPRINTF("[\033[38;5;33mℹ\033[0m] Heuristic EXO filter choice: %s choisit\n",
               (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
   }
   ```
   The evaluator detects either this codec-specific line or the generic fallback lines: `Filtre Kanzi EXE` or `Filtre BCJ`.

   Evaluator note:
   - For reliable evaluation (avoid SKIP rows), always add a codec-specific line with the exact shape:
     - `Heuristic <CODEC> filter choice: %s chosen`

5) Build:
   ```bash
   make -j packer
   ```

Notes:
- Do not alter stub/packer logic; only add the codec auto-decider and its branch/log.
- Keep the decider `static inline` where possible and use O(n) segment scans; memory O(1).
- Do not change the AUTO stub re-selection block. AUTO must select the filter, then the packer re-selects the proper stub for the chosen filter (this is required for consistent output sizes and correct runtime behavior).


## 5) AUTO vs dataset evaluation

Script: `tools/eval_codec_auto_vs_csv.sh`

### Purpose
Run the packer with `--exe-filter=auto` on every `path` in the dataset and compare the predicted filter against the `winner` column.

### Usage
```bash
./tools/eval_codec_auto_vs_csv.sh \
  --codec exo \
  --csv build/predictor_datasets/exo_deep.csv \
  --out build/predictor_evals/exo_eval_auto_vs_dataset.csv
```

### Output
- Summary on stderr:
  - `Evaluated: N | OK=... KO=... SKIP=... | ACC=... | AVG_REGRET=...`
- CSV `--out` with columns:
  - `path,pred,winner,correct,bcj_final,kanzi_final,regret`

### Skip conditions
- Missing or non-executable `path`
- Log did not contain a recognizable heuristic line

Tip: To avoid SKIPs, ensure `path` points to executables on this machine (e.g. `/usr/bin/...`).


## 6) Quickstart (replace <codec>)

```bash
# 1) Collect dataset
./tools/collect_codec_deep.sh --codec <codec> --out build/predictor_datasets/<codec>_deep.csv

# 2) Train + distill + export header
source .venv/bin/activate
python src/tools/ml/train_codec_predictor.py \
  --codec <codec> \
  --csv build/predictor_datasets/<codec>_deep.csv \
  --outdir build/predictor_models/<codec> \
  --distill-to-tree --export-dt-header \
  --symbol-prefix <codec> \
  --dt-max-depth 10 --dt-min-leaf 10

# 3) Integrate: include header + implement decide_exe_filter_auto_<codec>_model + wire branch + add verbose log
#   (edit src/packer/zelf_packer.c accordingly)

# 4) Build
make -j packer

# 5) Evaluate AUTO vs dataset
./tools/eval_codec_auto_vs_csv.sh \
  --codec <codec> \
  --csv build/predictor_datasets/<codec>_deep.csv \
  --out build/predictor_evals/<codec>_eval_auto_vs_dataset.csv
```


## 7) Troubleshooting

- High SKIP count at evaluation:
  - Ensure dataset `path` exists and is executable locally.
  - Ensure the codec-specific verbose line is present exactly as expected (see section 4.4).

- Type or symbol collisions when including multiple predictors:
  - Always export with `--symbol-prefix <codec>`; ensure the typedef is `<codec>_CodecFeat`.

- Python environment issues (training fails or `pip show` crashes):
  - Run the sanity checks from section 1.
  - If `pip` upgraded `numpy` to `2.x` while using `scikit-learn<2`, reinstall a compatible `numpy<2`.

- Feature mismatch:
  - Fill `in[PP_FEAT_*]` strictly per `src/packer/exe_predict_feature_index.h`.

- Performance concerns:
  - Keep scans linear and reuse helpers used by existing predictors.


## 8) Appendix

- Minimal columns required by evaluator: `path,winner,bcj_final,kanzi_final`.
- The dataset produced by `collect_codec_deep.sh` adds many more columns (super-strip, filtered lengths, stub sizes, ratios, timings, memory). They are not needed by the evaluator but remain useful for analysis.
