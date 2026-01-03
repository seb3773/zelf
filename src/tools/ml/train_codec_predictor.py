#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import annotations
"""
train_codec_predictor.py

Generic training pipeline for EXE filter predictor (BCJ vs KanziEXE) for any codec.
- Loads a dataset CSV produced by collect_codec_deep.sh (or codec-specific collectors)
- Builds target 'winner' (kanziexe vs bcj)
- Uses ONLY pre-filter features by default (safe for production inference)
- GroupKFold CV by basename(path)
- Compares models; selects best by accuracy then expected regret
- Optionally distills to a compact DecisionTree and exports a tiny C header for packer integration

Header export uses the common feature index mapping (exe_predict_feature_index.h), so
packer-side feature vectors remain consistent across codecs.
"""

import argparse
import json
import os
import sys
import warnings
from dataclasses import dataclass
from typing import Dict, List, Tuple

import numpy as np
import pandas as pd

from sklearn.model_selection import GroupKFold
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
from sklearn.linear_model import LogisticRegression
from sklearn.tree import DecisionTreeClassifier
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.metrics import accuracy_score

try:
    import m2cgen as m2c  # type: ignore
    HAVE_M2CGEN = True
except Exception:
    HAVE_M2CGEN = False

try:
    from xgboost import XGBClassifier  # type: ignore
    HAVE_XGB = True
except Exception:
    HAVE_XGB = False

EXCLUDED_SUBSTRS_DEFAULT = [
    # Post-filter metrics and derived ratios (not available at inference)
    'bcj_final', 'kanzi_final', 'bcj_ratio', 'kanzi_ratio',
    'bcj_elapsed', 'kanzi_elapsed', 'bcj_maxrss', 'kanzi_maxrss',
    'bcj_filtered_len', 'kanzi_filtered_len',
    'kexp', 'ratio_bcj', 'ratio_kanzi', 'delta_eff', 'comp_gain_k_vs_b_lz4hc',
    # Filtered-probe metrics (raw/bcj/kanzi streams)
    'ent_raw', 'ent_bcj', 'ent_kanzi',
    'autoc1_raw', 'autoc1_bcj', 'autoc1_kanzi',
    'entstd_raw', 'entstd_bcj', 'entstd_kanzi',
    'diffent_raw', 'diffent_bcj', 'diffent_kanzi',
    'lz4_', 'lz4hc_',
]

ALLOWED_EXTRA_PREFILTER: List[str] = []

LABEL_COL = 'winner'
PATH_COL = 'path'

# Whitelist of features the packer can compute at runtime (same as PP/BLZ/Zx7b)
PACKER_FEATS_WHITELIST = [
    'file_size', 'n_load', 'etype', 'has_interp',
    'text_sz', 'ro_sz', 'data_sz',
    'text_ratio', 'ro_ratio', 'data_ratio',
    'text_entropy', 'ro_entropy', 'data_entropy',
    'zeros_ratio_total', 'zero_runs_16', 'zero_runs_32',
    'ascii_ratio_rodata',
    'e8_cnt', 'e9_cnt', 'ff_calljmp_cnt', 'eb_cnt', 'jcc32_cnt',
    'branch_density_per_kb', 'riprel_estimate', 'nop_ratio_text', 'imm64_mov_cnt',
    'align_pad_ratio', 'ro_ptr_like_cnt',
    'rel32_disp_entropy8', 'rel32_intext_ratio', 'rel32_intext_cnt', 'max_rel32_abs',
    'avg_p_align_log2', 'max_p_align_log2',
    'x_load_cnt', 'ro_load_cnt', 'rw_load_cnt', 'bss_sz',
    'ret_cnt', 'rel_branch_ratio', 'avg_rel32_abs',
]

@dataclass
class ModelResult:
    name: str
    acc: float
    regret: float
    model: object

def _is_prefilter_column(col: str, excluded_substrs: List[str]) -> bool:
    lc = col.lower()
    if lc in (LABEL_COL, PATH_COL):
        return False
    for s in excluded_substrs:
        if s in lc:
            return False
    return True

def _select_feature_columns(df: pd.DataFrame, excluded_substrs: List[str], whitelist: List[str] | None = None) -> List[str]:
    cols: List[str] = []
    for c in df.columns:
        if c in ALLOWED_EXTRA_PREFILTER:
            cols.append(c)
            continue
        if not _is_prefilter_column(c, excluded_substrs):
            continue
        if whitelist is not None and c not in whitelist:
            continue
        if pd.api.types.is_numeric_dtype(df[c]):
            cols.append(c)
            continue
        try:
            pd.to_numeric(df[c]).head(1)
            cols.append(c)
        except Exception:
            pass
    return cols

def _build_target(df: pd.DataFrame) -> pd.Series:
    if LABEL_COL in df.columns and df[LABEL_COL].notna().any():
        return df[LABEL_COL].astype(str).str.strip().str.lower().map({'kanziexe': 1, 'bcj': 0})
    if {'bcj_final', 'kanzi_final'} <= set(df.columns):
        return (pd.to_numeric(df['kanzi_final'], errors='coerce') < pd.to_numeric(df['bcj_final'], errors='coerce')).astype(int)
    raise ValueError("No 'winner' column and cannot derive from final sizes.")

def _groups_from_path(df: pd.DataFrame) -> np.ndarray:
    if PATH_COL in df.columns:
        return df[PATH_COL].astype(str).apply(lambda p: os.path.basename(p)).values
    return np.arange(len(df))

def _expected_regret(y_true: np.ndarray, y_pred: np.ndarray, bcj_final: np.ndarray, kan_final: np.ndarray) -> float:
    best = np.minimum(bcj_final, kan_final)
    pred_size = np.where(y_pred == 1, kan_final, bcj_final)
    valid = np.isfinite(best) & np.isfinite(pred_size)
    if not np.any(valid):
        return float('nan')
    return float(np.mean(pred_size[valid] - best[valid]))

def _cv_evaluate(model, X: pd.DataFrame, y: pd.Series, groups: np.ndarray, bcj_final: np.ndarray, kan_final: np.ndarray, n_splits: int = 5) -> Tuple[float, float]:
    uniq = np.unique(groups)
    n_splits = min(n_splits, max(2, uniq.shape[0]))
    gkf = GroupKFold(n_splits=n_splits)
    accs: List[float] = []
    regrets: List[float] = []
    for tr, te in gkf.split(X, y, groups):
        Xtr, Xte = X.iloc[tr], X.iloc[te]
        ytr, yte = y.iloc[tr], y.iloc[te]
        m = model
        m.fit(Xtr, ytr)
        yp = m.predict(Xte)
        accs.append(accuracy_score(yte, yp))
        regrets.append(_expected_regret(yte.values, yp, bcj_final[te], kan_final[te]))
    return float(np.mean(accs)), float(np.nanmean(regrets))

def _make_models(random_state: int = 42) -> Dict[str, object]:
    models: Dict[str, object] = {}
    models['LogReg'] = Pipeline([
        ('scaler', StandardScaler(with_mean=False)),
        ('clf', LogisticRegression(max_iter=2000)),
    ])
    models['DecisionTree'] = DecisionTreeClassifier(random_state=random_state, max_depth=None, min_samples_leaf=5)
    models['RandomForest'] = RandomForestClassifier(random_state=random_state, n_estimators=300, max_depth=None, n_jobs=-1)
    models['GradBoost'] = GradientBoostingClassifier(random_state=random_state)
    if HAVE_XGB:
        models['XGB'] = XGBClassifier(
            n_estimators=400, max_depth=6, learning_rate=0.07,
            subsample=0.9, colsample_bytree=0.9, reg_lambda=1.0,
            n_jobs=-1, tree_method='hist', random_state=random_state,
        )
    return models

def export_decision_tree_to_c(dt: DecisionTreeClassifier, feat_names: List[str], out_path: str,
                              func_prefix: str = 'codec', emit_wrapper: bool = True) -> None:
    """Export a DecisionTreeClassifier as a compact C header using PP_FEAT_* indices.

    func_prefix controls symbol names: {prefix}_dt_core and {prefix}_dt_predict_from_pvec
    """
    tree_ = dt.tree_
    features = tree_.feature
    thresholds = tree_.threshold

    used_idx = sorted(set(int(i) for i in features if i >= 0))
    used_feats = [feat_names[i] for i in used_idx]

    def node_to_c(node: int, indent: int = 2) -> str:
        sp = ' ' * indent
        if tree_.feature[node] < 0:
            values = tree_.value[node][0]
            klass = int(np.argmax(values))  # 0=BCJ, 1=KANZIEXE
            return f"{sp}return {klass};\n"
        fname = feat_names[tree_.feature[node]]
        thr = thresholds[node]
        code = f"{sp}if (m->{fname} <= {thr:.9f}) {{\n"
        code += node_to_c(tree_.children_left[node], indent + 2)
        code += f"{sp}}} else {{\n"
        code += node_to_c(tree_.children_right[node], indent + 2)
        code += f"{sp}}}\n"
        return code

    header: List[str] = []
    header.append("// Auto-generated by tools/ml/train_codec_predictor.py\n")
    header.append("#pragma once\n")
    header.append("// Returns 0 for BCJ, 1 for KANZIEXE\n")
    header.append("#ifdef __cplusplus\nextern \"C\" {\n#endif\n")
    header.append("#include \"exe_predict_feature_index.h\"\n")
    header.append("\n// Minimal feature struct (only used fields)\n")
    header.append(f"typedef struct {{\n")
    for fn in used_feats:
        header.append(f"    double {fn};\n")
    header.append(f"}} {func_prefix}_CodecFeat;\n\n")

    core_name = f"{func_prefix}_dt_core"
    wrap_name = f"{func_prefix}_dt_predict_from_pvec"

    header.append(f"static inline int {core_name}(const {func_prefix}_CodecFeat *m) {{\n")
    header.append(node_to_c(0, indent=2))
    header.append("}\n\n")

    if emit_wrapper:
        header.append(f"static inline int {wrap_name}(const double *in) {{\n")
        header.append(f"    {func_prefix}_CodecFeat m; memset(&m, 0, sizeof(m));\n")
        for fn in used_feats:
            cname = ''.join(ch if (ch.isalnum() or ch=='_') else '_' for ch in fn)
            header.append(f"    m.{fn} = in[PP_FEAT_{cname}];\n")
        header.append(f"    return {core_name}(&m);\n")
        header.append("}\n\n")
    header.append("#ifdef __cplusplus\n}\n#endif\n")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w') as f:
        f.write(''.join(header))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--codec', required=True, help='Codec name (e.g., blz, zx7b, pp)')
    ap.add_argument('--csv', required=True, help='Path to dataset CSV')
    ap.add_argument('--outdir', required=True, help='Output directory for reports')
    ap.add_argument('--allow-post-filter-features', action='store_true', default=False)
    ap.add_argument('--use-packer-whitelist', action='store_true', default=True)
    ap.add_argument('--force-model', type=str, default='')
    ap.add_argument('--distill-to-tree', action='store_true', default=False)
    ap.add_argument('--dt-max-depth', type=int, default=10)
    ap.add_argument('--dt-min-leaf', type=int, default=10)
    ap.add_argument('--export-dt-header', action='store_true', default=False)
    ap.add_argument('--symbol-prefix', type=str, default='')
    ap.add_argument('--header-path', type=str, default='')
    ap.add_argument('--small-model', action='store_true', default=False)
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)

    df = pd.read_csv(args.csv)
    if df.empty:
        print('Empty CSV, abort.', file=sys.stderr)
        sys.exit(2)

    y = _build_target(df)
    mask = y.notna()
    df = df[mask].reset_index(drop=True)
    y = y[mask].astype(int).reset_index(drop=True)

    bcj_final = pd.to_numeric(df.get('bcj_final', np.nan), errors='coerce').to_numpy()
    kan_final = pd.to_numeric(df.get('kanzi_final', np.nan), errors='coerce').to_numpy()

    excluded = [] if args.allow_post_filter_features else EXCLUDED_SUBSTRS_DEFAULT
    whitelist = PACKER_FEATS_WHITELIST if args.use_packer_whitelist else None
    feat_cols = _select_feature_columns(df, excluded, whitelist)
    feat_cols = [c for c in feat_cols if c not in (LABEL_COL, PATH_COL)]

    X = df[feat_cols].apply(pd.to_numeric, errors='coerce').fillna(0.0)
    groups = _groups_from_path(df)

    models = _make_models()
    if args.small_model:
        models['RandomForest'] = RandomForestClassifier(random_state=42, n_estimators=48, max_depth=4, n_jobs=-1)
        if HAVE_XGB:
            from xgboost import XGBClassifier  # type: ignore
            models['XGB'] = XGBClassifier(
                n_estimators=60, max_depth=3, learning_rate=0.08,
                subsample=0.9, colsample_bytree=0.9, reg_lambda=1.0,
                n_jobs=-1, tree_method='hist', random_state=42,
            )

    results: List[ModelResult] = []
    if args.force_model:
        name = args.force_model
        if name not in models:
            print(f"Unknown --force-model '{name}'. Choices: {list(models.keys())}", file=sys.stderr)
            sys.exit(4)
        model = models[name]
        acc, rgt = _cv_evaluate(model, X, y, groups, bcj_final, kan_final)
        results.append(ModelResult(name, acc, rgt, model))
        print(f"{name:12s} | acc={acc:.4f} | regret={rgt:.2f}")
        best = results[0]
        best.model.fit(X, y)
    else:
        for name, model in models.items():
            try:
                acc, rgt = _cv_evaluate(model, X, y, groups, bcj_final, kan_final)
                results.append(ModelResult(name, acc, rgt, model))
                print(f"{name:12s} | acc={acc:.4f} | regret={rgt:.2f}")
            except Exception as e:
                warnings.warn(f"Model {name} failed: {e}")
        if not results:
            print('No successful model evaluations.', file=sys.stderr)
            sys.exit(3)
        results.sort(key=lambda mr: (-mr.acc, mr.regret if np.isfinite(mr.regret) else np.inf))
        best = results[0]
        print(f"\nBest model: {best.name} (acc={best.acc:.4f}, regret={best.regret:.2f})")
        best.model.fit(X, y)

    report = {
        'codec': args.codec,
        'best_model': best.name,
        'accuracy': best.acc,
        'expected_regret': best.regret,
        'n_samples': int(X.shape[0]),
        'n_features': int(X.shape[1]),
        'features': feat_cols,
        'csv': os.path.abspath(args.csv),
    }
    with open(os.path.join(args.outdir, 'report.json'), 'w') as f:
        json.dump(report, f, indent=2)
    with open(os.path.join(args.outdir, 'features.txt'), 'w') as f:
        f.write('\n'.join(feat_cols))

    # Export a compact DecisionTree header if requested
    if args.distill_to_tree or args.export_dt_header:
        # Fit a distilled DT on best predictions or train DT directly on labels
        if args.distill_to_tree:
            best.model.fit(X, y)
            y_hat = best.model.predict(X)
            dt = DecisionTreeClassifier(random_state=42, max_depth=args.dt_max_depth, min_samples_leaf=args.dt_min_leaf)
            dt.fit(X, y_hat)
            acc_dt = float(accuracy_score(y, dt.predict(X)))
            print(f"Distilled DT (depth={args.dt_max_depth}, leaf={args.dt_min_leaf}) | acc_vs_true={acc_dt:.4f}")
        else:
            if isinstance(best.model, DecisionTreeClassifier):
                dt = best.model
            else:
                dt = DecisionTreeClassifier(random_state=42, max_depth=args.dt_max_depth, min_samples_leaf=args.dt_min_leaf)
                dt.fit(X, y)
        # Output header path and prefix defaults
        prefix = args.symbol_prefix if args.symbol_prefix else args.codec.lower()
        out_header_dt = args.header_path if args.header_path else os.path.join('src', 'packer', f'{prefix}_predict_dt.h')
        export_decision_tree_to_c(dt, feat_cols, out_header_dt, func_prefix=prefix, emit_wrapper=True)
        print(f"Exported compact DT header: {out_header_dt}")

if __name__ == '__main__':
    main()
