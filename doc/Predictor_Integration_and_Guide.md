# Predictor integration (LZAV, DoboZ, ZSTD) and model creation guide

This document tracks the integration of EXE filter prediction models (BCJ vs KanziEXE) for LZAV, DoboZ, and ZSTD, and provides a reproducible procedure to train and integrate new models from a CSV dataset.

## Summary of integrated models

- **[LZAV]**
  - Dataset: `build/predictor_datasets/lzav_deep.csv`
  - Training + distillation + exported header: `src/packer/lzav_predict_dt.h`
  - Integration in `src/packer/zelf_packer.c`:
    - `#include "lzav_predict_dt.h"`
    - New function: `decide_exe_filter_auto_lzav_model()`
      - Walks ELF segments (TEXT/RO/DATA), extracts features (entropies, E8/E9/JCC/FF-call/jmp counts, RIPREL, NOPs, ratios, etc.).
      - Fills `PP_FEAT_*` indices (including `PP_FEAT_etype` via `Elf64_Ehdr.e_type`).
      - Calls `lzav_dt_predict_from_pvec(in)` (returns 0=BCJ, 1=KanziExe).
    - AUTO wiring: branch `else if (use_lzav)` → `decide_exe_filter_auto_lzav_model(...)`.
    - Heuristic log: “Heuristic LZAV filter choice: …”.
  - Evaluation: `tools/eval_codec_auto_vs_csv.sh --codec lzav --csv build/predictor_datasets/lzav_deep.csv`
    - Result: `Evaluated: 2067 | OK=2018 KO=49 SKIP=0 | ACC=0.9763 | AVG_REGRET=243.00`
    - CSV: `build/predictor_evals/lzav_eval_auto_vs_dataset.csv`

- **[DoboZ]**
  - Dataset: `build/predictor_datasets/doboz_deep.csv`
  - Training + distillation + exported header: `src/packer/doboz_predict_dt.h`
  - Integration in `src/packer/zelf_packer.c`:
    - `#include "doboz_predict_dt.h"`
    - New function: `decide_exe_filter_auto_doboz_model()` (feature pipeline identical to LZ4/LZAV).
    - AUTO wiring: `else if (use_doboz)` → DoboZ model.
    - Heuristic log: “Heuristic DoboZ filter choice: …”.
  - Evaluation: `tools/eval_codec_auto_vs_csv.sh --codec doboz --csv build/predictor_datasets/doboz_deep.csv`
    - Result: `Evaluated: 2067 | OK=1935 KO=132 SKIP=0 | ACC=0.9361 | AVG_REGRET=494.33`
    - CSV: `build/predictor_evals/doboz_eval_auto_vs_dataset.csv`

- **[ZSTD]**
  - Dataset: `build/predictor_datasets/zstd_deep.csv`
  - Training + distillation + exported header: `src/packer/zstd_predict_dt.h`
  - Integration in `src/packer/zelf_packer.c`:
    - `#include "zstd_predict_dt.h"`
    - New function: `decide_exe_filter_auto_zstd_model()` (same pipeline).
    - AUTO wiring: `else if (use_zstd)` → ZSTD model.
    - Heuristic log: “Heuristic ZSTD filter choice: …”.
  - Evaluation: `tools/eval_codec_auto_vs_csv.sh --codec zstd --csv build/predictor_datasets/zstd_deep.csv`
    - Result: `Evaluated: 2067 | OK=1947 KO=120 SKIP=0 | ACC=0.9419 | AVG_REGRET=253.66`
    - CSV: `build/predictor_evals/zstd_eval_auto_vs_dataset.csv`

- **[Snappy]**
  - Dataset: `build/predictor_datasets/snappy_deep.csv`
  - Training + distillation + exported header: `src/packer/snappy_predict_dt.h`
  - Integration in `src/packer/zelf_packer.c`:
    - `#include "snappy_predict_dt.h"`
    - New function: `decide_exe_filter_auto_snappy_model()` (same pipeline).
    - AUTO wiring: `else if (use_snappy)` → Snappy model.
    - Heuristic log: “Heuristic SNAPPY filter choice: …”.
  - Evaluation: `tools/eval_codec_auto_vs_csv.sh --codec snappy --csv build/predictor_datasets/snappy_deep.csv`
    - Result: `Evaluated: 2067 | OK=2001 KO=66 SKIP=0 | ACC=0.9681 | AVG_REGRET=228.75`
    - CSV: `build/predictor_evals/snappy_eval_auto_vs_dataset.csv`

- **[Apultra]**
  - Dataset: `build/predictor_datasets/apultra_deep.csv`
  - Training + distillation + exported header: `src/packer/apultra_predict_dt.h`
  - Integration in `src/packer/zelf_packer.c`:
    - `#include "apultra_predict_dt.h"`
    - New function: `decide_exe_filter_auto_apultra_model()` (same pipeline).
    - AUTO wiring: `else if (use_apultra)` → Apultra model.
    - Heuristic log: “Heuristic APULTRA filter choice: …”.
  - Evaluation: `tools/eval_codec_auto_vs_csv.sh --codec apultra --csv build/predictor_datasets/apultra_deep.csv`
    - Result: `Evaluated: 2067 | OK=1917 KO=150 SKIP=0 | ACC=0.9274 | AVG_REGRET=406.34`
    - CSV: `build/predictor_evals/apultra_eval_auto_vs_dataset.csv`

- **[Density]**
  - Dataset: `build/predictor_datasets/density_deep.csv`
  - Training + distillation + exported header: `src/packer/density_predict_dt.h`
  - Integration in `src/packer/zelf_packer.c`:
    - `#include "density_predict_dt.h"`
    - New function: `decide_exe_filter_auto_density_model()` (same pipeline).
    - AUTO wiring: `else if (use_density)` → Density model.
    - Heuristic log: “Heuristic DENSITY filter choice: …”.
  - Evaluation: `tools/eval_codec_auto_vs_csv.sh --codec density --csv build/predictor_datasets/density_deep.csv`
    - Result: `Evaluated: 1513 | OK=1498 KO=15 SKIP=0 | ACC=0.9901 | AVG_REGRET=7.33`
    - CSV: `build/predictor_evals/density_eval_auto_vs_dataset.csv`

- **[NZ1]**
  - Dataset: `build/predictor_datasets/nz1_deep.csv`
  - Training + distillation + exported header: `src/packer/nz1_predict_dt.h`
  - Model/report directory: `build/predictor_models/nz1/`
    - `report.json`: `accuracy=0.9715`, `expected_regret=241.88` (bytes)
  - Integration:
    - Packer AUTO branch calls `decide_exe_filter_auto_nz1_model(...)` which runs `nz1_dt_predict_from_pvec(in)`.
  - Evaluation:
    - `tools/eval_codec_auto_vs_csv.sh --codec nz1 --csv build/predictor_datasets/nz1_deep.csv --out build/predictor_evals/nz1_eval_auto_vs_dataset.csv`

- **[LZFSE]**
  - Dataset: `build/predictor_datasets/lzfse_deep.csv`
  - Training + distillation + exported header: `src/packer/lzfse_predict_dt.h`
  - Model/report directory: `build/predictor_models/lzfse/`
    - `report.json`: `best_model=RandomForest`, `accuracy=0.9498`, `expected_regret=133.77` (bytes)
  - Integration:
    - Packer AUTO branch calls `decide_exe_filter_auto_lzfse_model(...)` which runs `lzfse_dt_predict_from_pvec(in)`.
  - Evaluation:
    - `tools/eval_codec_auto_vs_csv.sh --codec lzfse --csv build/predictor_datasets/lzfse_deep.csv --out build/predictor_evals/lzfse_eval_auto_vs_dataset.csv`

- **[CSC]**
  - Dataset: `build/predictor_datasets/csc_deep.csv`
  - Training + distillation + exported header: `src/packer/csc_predict_dt.h`
  - Model/report directory: `build/predictor_models/csc/`
    - `report.json`: `best_model=RandomForest`, `accuracy=0.9830`, `expected_regret=3.54` (bytes)
  - Integration:
    - Packer AUTO branch calls `decide_exe_filter_auto_csc_model(...)` which runs `csc_dt_predict_from_pvec(in)`.
  - Evaluation:
    - `tools/eval_codec_auto_vs_csv.sh --codec csc --csv build/predictor_datasets/csc_deep.csv --out build/predictor_evals/csc_eval_auto_vs_dataset.csv`

- **Typedef note**: auto-generated headers often use `CodecFeat`. Because multiple headers are included in `zelf_packer.c`, rename to a unique name per codec if needed:
  - `src/packer/lzav_predict_dt.h`: `CodecFeat` → `lzav_CodecFeat`
  - `src/packer/doboz_predict_dt.h`: `CodecFeat` → `doboz_CodecFeat`
  - `src/packer/zstd_predict_dt.h`: `CodecFeat` → `zstd_CodecFeat`
  - `src/packer/snappy_predict_dt.h`: `CodecFeat` → `snappy_CodecFeat`
  - `src/packer/apultra_predict_dt.h`: `CodecFeat` → `apultra_CodecFeat`

## Guide: building a prediction model (CSV dataset → C header)

### 1) Prerequisites

- Python 3.8+
- ML tools in `src/tools/ml/train_codec_predictor.py`
- Python deps (venv recommended):

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install "numpy>=1.23,<2" "pandas>=1.5,<3" "scikit-learn>=1.2,<2"
```

### 2) Dataset generation

- Use the collector: `tools/collect_codec_deep.sh`
- CSV output: `build/predictor_datasets/<codec>_deep.csv`

Examples already produced: `build/predictor_datasets/lzav_deep.csv`, `doboz_deep.csv`, `zstd_deep.csv`.

### 3) Training + distillation + header export

- Typical command (adapt `<codec>`):

```bash
. .venv/bin/activate
python src/tools/ml/train_codec_predictor.py \
  --codec <codec> \
  --csv build/predictor_datasets/<codec>_deep.csv \
  --outdir build/predictor_models/<codec> \
  --distill-to-tree --export-dt-header \
  --symbol-prefix <codec> \
  --dt-max-depth 10 --dt-min-leaf 10
```

- The compact DecisionTree header is written to: `src/packer/<codec>_predict_dt.h`
- Tips:
  - Tune `--dt-max-depth` and `--dt-min-leaf` to balance `ACC` / `AVG_REGRET`.
  - If the header emits `typedef CodecFeat`, rename it to `<codec>_CodecFeat` to avoid collisions when multiple headers are included.

### 4) Integration in `zelf_packer.c`

1. Add the include at the top:
   ```c
   #include "<codec>_predict_dt.h"
   ```
2. Implement `decide_exe_filter_auto_<codec>_model(...)` by copying the existing pipeline (LZ4/LZAV/DoboZ/ZSTD):
   - Iterate `PT_LOAD` segments and compute counters/entropies.
   - Build `double in[PP_FEAT_COUNT]` using `PP_FEAT_*` indices from `src/packer/exe_predict_feature_index.h`.
   - Call `<codec>_dt_predict_from_pvec(in)` and return `EXE_FILTER_BCJ` (0) or `EXE_FILTER_KANZIEXE` (1).
3. Wire AUTO selection: in the AUTO block, add `else if (use_<codec>) { auto_sel = decide_exe_filter_auto_<codec>_model(...); }`.
4. Add an info log:
   ```c
   VPRINTF("[\033[38;5;33mℹ\033[0m] Heuristic <CODEC> filter choice: %s selected\n",
           (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
   ```

### 5) Build and evaluation

```bash
make -j
./tools/eval_codec_auto_vs_csv.sh --codec <codec> \
  --csv build/predictor_datasets/<codec>_deep.csv \
  --out build/predictor_evals/<codec>_eval_auto_vs_dataset.csv
```

- Expected output: `ACC`, `AVG_REGRET` summary and CSV in `build/predictor_evals/`.

## Technical notes

- Features and indices are defined in `src/packer/exe_predict_feature_index.h`.
- The feature pipeline is common: histograms, entropies (`TEXT/RO/DATA`), branch density, opcode counters (`E8/E9/JCC/EB/FF call&jmp`), RIP-relative estimates, ASCII/zero ratios, zero runs, `rel32` metrics (mean/max magnitude, target ratio in `TEXT`, displacement top-byte entropy), NOPs, alignment padding.
- For LZAV, `PP_FEAT_etype` is also filled (from the ELF header) because the tree uses it.
- For production, follow the recommended compiler/linker flags to optimize size/perf.

## Quick reproducibility (examples)

- LZAV:
```bash
. .venv/bin/activate
python src/tools/ml/train_codec_predictor.py --codec lzav --csv build/predictor_datasets/lzav_deep.csv \
  --outdir build/predictor_models/lzav --distill-to-tree --export-dt-header \
  --symbol-prefix lzav --dt-max-depth 10 --dt-min-leaf 10
make -j
./tools/eval_codec_auto_vs_csv.sh --codec lzav --csv build/predictor_datasets/lzav_deep.csv \
  --out build/predictor_evals/lzav_eval_auto_vs_dataset.csv
```
- DoboZ:
```bash
. .venv/bin/activate
python src/tools/ml/train_codec_predictor.py --codec doboz --csv build/predictor_datasets/doboz_deep.csv \
  --outdir build/predictor_models/doboz --distill-to-tree --export-dt-header \
  --symbol-prefix doboz --dt-max-depth 10 --dt-min-leaf 10
make -j
./tools/eval_codec_auto_vs_csv.sh --codec doboz --csv build/predictor_datasets/doboz_deep.csv \
  --out build/predictor_evals/doboz_eval_auto_vs_dataset.csv
```
- ZSTD:
```bash
. .venv/bin/activate
python src/tools/ml/train_codec_predictor.py --codec zstd --csv build/predictor_datasets/zstd_deep.csv \
  --outdir build/predictor_models/zstd --distill-to-tree --export-dt-header \
  --symbol-prefix zstd --dt-max-depth 10 --dt-min-leaf 10
make -j
./tools/eval_codec_auto_vs_csv.sh --codec zstd --csv build/predictor_datasets/zstd_deep.csv \
  --out build/predictor_evals/zstd_eval_auto_vs_dataset.csv
```

- Density:
```bash
. .venv/bin/activate
python src/tools/ml/train_codec_predictor.py --codec density --csv build/predictor_datasets/density_deep.csv \
  --outdir build/predictor_models/density --distill-to-tree --export-dt-header \
  --symbol-prefix density --dt-max-depth 10 --dt-min-leaf 10
make -j
./tools/eval_codec_auto_vs_csv.sh --codec density --csv build/predictor_datasets/density_deep.csv \
  --out build/predictor_evals/density_eval_auto_vs_dataset.csv
```

## Next steps

- Integrate additional codecs as datasets become available (same procedure). Optionally produce an error/regret report to target the worst offenders.
