#!/usr/bin/env bash
set -euo pipefail
# eval_codec_auto_vs_csv.sh
# Compare packer AUTO selection (for a given codec) against dataset 'winner' column.
# Outputs per-file CSV and a summary accuracy/regret on stderr.
#
# Usage:
#   tools/eval_codec_auto_vs_csv.sh --codec <codec> --csv <dataset.csv> [--out <out.csv>] [--limit N]
#
CODEC=""
CSV=""
OUT=""
LIMIT=0

usage(){
  echo "Usage: $0 --codec <lz4|apultra|zx7b|zx0|blz|pp|zstd|lzma|qlz|snappy|doboz|lzav|exo|shrinkler> --csv <csv> [--out <out.csv>] [--limit N]" >&2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --codec) CODEC="${2:-}"; shift 2;;
    --csv) CSV="${2:-}"; shift 2;;
    --out) OUT="${2:-}"; shift 2;;
    --limit) LIMIT="${2:-0}"; shift 2;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown arg: $1" >&2; usage; exit 1;;
  esac
done

[[ -n "$CODEC" ]] || { echo "--codec is required" >&2; usage; exit 1; }
[[ -n "$CSV" ]] || { usage; exit 1; }
[[ -f "$CSV" ]] || { echo "CSV not found: $CSV" >&2; exit 2; }
[[ -x build/zelf ]] || { echo "build/zelf missing. Run 'make'." >&2; exit 2; }

[[ -n "$OUT" ]] || OUT="build/predictor_evals/${CODEC}_eval_auto_vs_dataset.csv"
mkdir -p "$(dirname -- "$OUT")"

echo "path,pred,winner,correct,bcj_final,kanzi_final,regret" > "$OUT"

i_path=-1; i_winner=-1; i_bcj=-1; i_kan=-1

# Determine header indices (simple CSV: comma-separated, no embedded commas)
IFS= read -r HEADER < "$CSV" || true
idx=0
OIFS=$IFS; IFS=','
for col in $HEADER; do
  lc=$(echo "$col" | tr 'A-Z' 'a-z')
  case "$lc" in
    path) i_path=$idx;;
    winner) i_winner=$idx;;
    bcj_final) i_bcj=$idx;;
    kanzi_final) i_kan=$idx;;
  esac
  idx=$((idx+1))
done
IFS=$OIFS

if [[ $i_path -lt 0 || $i_winner -lt 0 || $i_bcj -lt 0 || $i_kan -lt 0 ]]; then
  echo "Missing required columns in CSV header (need: path,winner,bcj_final,kanzi_final)" >&2
  exit 3
fi

total=0
ok=0
ko=0
skipped=0
sum_regret=0
cnt_regret=0

line_no=0
while IFS= read -r line || [[ -n "$line" ]]; do
  line_no=$((line_no+1))
  # skip header
  if [[ $line_no -eq 1 ]]; then continue; fi
  # split
  IFS=',' read -r -a A <<< "$line"
  # guard
  [[ ${#A[@]} -gt $i_kan ]] || { skipped=$((skipped+1)); continue; }
  path="${A[$i_path]}"
  # strip surrounding quotes if present
  if [[ "$path" == '"'*'"' ]]; then
    path="${path#\"}"
    path="${path%\"}"
  fi
  # trim whitespace
  path="${path## }"; path="${path%% }"
  winner=$(echo "${A[$i_winner]}" | tr 'A-Z' 'a-z')
  bcj_final="${A[$i_bcj]}"
  kan_final="${A[$i_kan]}"
  [[ -n "$path" && -x "$path" ]] || { skipped=$((skipped+1)); continue; }

  total=$((total+1))

  # run packer auto (write output to /tmp/collect_data_packed)
  rm -f /tmp/collect_data_packed 2>/dev/null || true
  if command -v timeout >/dev/null 2>&1; then
    LOG=$( (timeout 600 ./build/zelf -"$CODEC" --exe-filter=auto --output"/tmp/collect_data_packed" --verbose "$path") 2>&1 || true )
  else
    LOG=$( (./build/zelf -"$CODEC" --exe-filter=auto --output"/tmp/collect_data_packed" --verbose "$path") 2>&1 || true )
  fi

  pred=""
  # Try codec-specific heuristic log first (e.g., BLZ current format)
  if echo "$LOG" | grep -Eqi "Heuristic +${CODEC^^} +filter +choice: *Kanzi"; then
    pred="kanziexe"
  elif echo "$LOG" | grep -Eqi "Heuristic +${CODEC^^} +filter +choice: *BCJ"; then
    pred="bcj"
  # Generic fallback: detect filter lines
  elif echo "$LOG" | grep -Eqi 'Filtre +Kanzi +EXE'; then
    pred="kanziexe"
  elif echo "$LOG" | grep -Eqi 'Filtre +BCJ'; then
    pred="bcj"
  else
    skipped=$((skipped+1))
    continue
  fi

  correct=0
  if [[ "$winner" == "kanziexe" || "$winner" == "bcj" ]]; then
    if [[ "$pred" == "$winner" ]]; then correct=1; fi
  fi
  if [[ $correct -eq 1 ]]; then ok=$((ok+1)); else ko=$((ko+1)); fi

  regret=""
  if [[ -n "$bcj_final" && -n "$kan_final" ]]; then
    bf=${bcj_final%%.*}
    kf=${kan_final%%.*}
    if [[ "$bf" =~ ^[0-9]+$ && "$kf" =~ ^[0-9]+$ ]]; then
      best=$(( bf < kf ? bf : kf ))
      pred_size=$bf
      if [[ "$pred" == "kanziexe" ]]; then pred_size=$kf; fi
      reg=$(( pred_size - best ))
      regret="$reg"
      sum_regret=$(( sum_regret + reg ))
      cnt_regret=$(( cnt_regret + 1 ))
    fi
  fi

  echo "$path,$pred,$winner,$correct,$bcj_final,$kan_final,$regret" >> "$OUT"

  if [[ $LIMIT -gt 0 && $total -ge $LIMIT ]]; then break; fi

done < "$CSV"

acc="0.0"
if [[ $total -gt 0 ]]; then
  acc=$(awk -v ok="$ok" -v tot="$total" 'BEGIN{printf("%.4f", (tot>0? ok/tot : 0))}')
fi
avg_regret=""
if [[ $cnt_regret -gt 0 ]]; then
  avg_regret=$(awk -v s="$sum_regret" -v n="$cnt_regret" 'BEGIN{printf("%.2f", s/n)}')
fi

echo "Evaluated: $total | OK=$ok KO=$ko SKIP=$skipped | ACC=$acc | AVG_REGRET=${avg_regret:-NA}" >&2
