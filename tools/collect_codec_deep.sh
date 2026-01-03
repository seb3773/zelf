#!/usr/bin/env bash
set -euo pipefail
set +x
# collect_codec_deep.sh
# Generic collector to build an extended dataset to study BCJ vs KanziEXE decisions
# for any supported codec of the packer. Mirrors collect_blz_deep.sh behavior.
#
# For each ELF in a directory, records:
#  - elfz_probe feature row
#  - filtered_probe row (post-filter metrics, agnostic of codec)
#  - super-strip input/output sizes
#  - BCJ filtered length and Kanzi filtered length
#  - Stub sizes (BCJ vs Kanzi)
#  - Final sizes and ratios for the selected codec
#  - Elapsed time (wall) and MAXRSS for both runs
#  - Winner label (kanziexe/bcj)
#
# Requirements: build/zelf, build/tools/elfz_probe, build/tools/filtered_probe
# Default timeout per run: 10 minutes

usage() {
  echo "Usage: $0 --codec <lz4|apultra|zx7b|zx0|blz|pp|zstd|lzma|qlz|snappy|doboz|lzav|exo|shrinkler|dsty> [--dir <dir>] --out <csv> [--limit N]" >&2
  echo "  dir: directory to scan (default: /usr/bin)" >&2
}

DIR="/usr/bin"
OUT=""
CODEC=""
LIMIT=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --codec) CODEC="${2:-}"; shift 2;;
    --dir) DIR="${2:-}"; shift 2;;
    --out) OUT="${2:-}"; shift 2;;
    --limit) LIMIT="${2:-0}"; shift 2;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown arg: $1" >&2; usage; exit 1;;
  esac
done

[[ -n "$CODEC" ]] || { echo "--codec is required" >&2; usage; exit 1; }
[[ -n "$OUT" ]] || { usage; exit 1; }

if [[ ! -x build/zelf ]]; then
  echo "Error: build/zelf not found. Build the packer first (make)." >&2
  exit 1
fi

if [[ ! -x build/tools/elfz_probe || ! -x build/tools/filtered_probe ]]; then
  echo "Info: tools missing; building via 'make tools'..." >&2
  make -s tools || { echo "Failed to build tools" >&2; exit 1; }
fi

# Prepare CSV header (append-safe)
if [[ ! -f "$OUT" ]]; then
  hdr_features=$(build/tools/elfz_probe --header)
  hdr_filtered=$(build/tools/filtered_probe --header --no-path)
  hdr_extras="sstrip_in,sstrip_out,sstrip_saved,bcj_filtered_len,kanzi_filtered_len,bcj_stub_size,kanzi_stub_size,bcj_final,bcj_ratio,kanzi_final,kanzi_ratio,bcj_elapsed_s,kanzi_elapsed_s,bcj_maxrss_kb,kanzi_maxrss_kb,winner"
  printf '%s,%s,%s\n' "$hdr_features" "$hdr_filtered" "$hdr_extras" > "$OUT"
fi

export LC_ALL=C

# Helper: parse last match
get_last() { sed -nE "$1" | tail -n1; }

# Resume support: read existing CSV and remember which paths were processed
declare -A DONE=()
already=0
i_path=-1
if [[ -f "$OUT" ]]; then
  IFS= read -r HEADER < "$OUT" || true
  idx=0
  OIFS=$IFS; IFS=','
  for col in $HEADER; do
    lc=$(echo "$col" | tr 'A-Z' 'a-z')
    if [[ "$lc" == "path" ]]; then i_path=$idx; fi
    idx=$((idx+1))
  done
  IFS=$OIFS
  if [[ $i_path -ge 0 ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
      [[ -z "$line" ]] && continue
      IFS=',' read -r -a A <<< "$line"
      [[ ${#A[@]} -gt $i_path ]] || continue
      p="${A[$i_path]}"
      if [[ "$p" == '"'*'"' ]]; then p="${p#\"}"; p="${p%\"}"; fi
      p="${p## }"; p="${p%% }"
      [[ -n "$p" ]] || continue
      DONE["$p"]=1
    done < <(tail -n +2 "$OUT")
  fi
fi

# Pre-compute eligible files using elfz_probe and x86_64 check
echo "Calculating real ELF binaries total count..." >&2
FILES=()
while IFS= read -r -d '' f; do
  # Probe features to ensure it's an ELF we can handle
  feats=$(build/tools/elfz_probe "$f" 2>/dev/null || true)
  [[ -z "$feats" ]] && continue
  # Enforce ELF64 x86-64
  hdr=$(LC_ALL=C readelf -h "$f" 2>/dev/null || true)
  if [[ -z "$hdr" ]] || ! grep -qE "Class:\\s*ELF64" <<<"$hdr" || ! grep -qEi "Machine:\\s*(Advanced Micro Devices X86-64|x86-64)" <<<"$hdr"; then
    continue
  fi
  # Only executable
  [[ -x "$f" ]] || continue
  FILES+=("$f")
done < <(find "$DIR" -maxdepth 1 -type f -size +10k -executable -print0)

TOTAL_ELF=${#FILES[@]}
ALREADY=0
if [[ $i_path -ge 0 && -f "$OUT" ]]; then
  for f in "${FILES[@]}"; do
    if [[ -n "${DONE[$f]+_}" ]]; then ALREADY=$((ALREADY+1)); fi
  done
fi

# Helper: run packer with a given filter for selected codec
run_pack() {
  local filter="$1"; shift
  local path="$1"
  local out
  local start_ts end_ts elapsed
  start_ts=$(date +%s.%N 2>/dev/null || date +%s)
  if command -v timeout >/dev/null 2>&1; then
    if [[ -x /usr/bin/time ]]; then
      out=$( ( /usr/bin/time -f 'ELAPSED=%e\nMAXRSS=%M' timeout 10m ./build/zelf -"$CODEC" --exe-filter="$filter" --output"/tmp/collect_data_packed" --verbose "$path" ) 2>&1 || true )
    else
      out=$( ( timeout 10m ./build/zelf -"$CODEC" --exe-filter="$filter" --output"/tmp/collect_data_packed" --verbose "$path" ) 2>&1 || true )
    fi
  else
    if [[ -x /usr/bin/time ]]; then
      out=$( ( /usr/bin/time -f 'ELAPSED=%e\nMAXRSS=%M' ./build/zelf -"$CODEC" --exe-filter="$filter" --output"/tmp/collect_data_packed" --verbose "$path" ) 2>&1 || true )
    else
      out=$( ( ./build/zelf -"$CODEC" --exe-filter="$filter" --output"/tmp/collect_data_packed" --verbose "$path" ) 2>&1 || true )
    fi
  fi
  end_ts=$(date +%s.%N 2>/dev/null || date +%s)
  if ! grep -q '^ELAPSED=' <<<"$out"; then
    elapsed=$(awk -v s="$start_ts" -v e="$end_ts" 'BEGIN{printf("%.3f", (e-s))}' 2>/dev/null || echo "")
    out+=$'\n'"ELAPSED=${elapsed}"
  fi
  if ! grep -q '^MAXRSS=' <<<"$out"; then
    out+=$'\n'"MAXRSS="
  fi
  printf '%s' "$out"
}

count=0  # newly processed in this session
for f in "${FILES[@]}"; do
  # Optional limit applies to newly processed
  if [[ $LIMIT -gt 0 && $count -ge $LIMIT ]]; then break; fi

  # Skip if already present in CSV
  if [[ -n "${DONE[$f]+_}" ]]; then
    continue
  fi

  # Probe features (row) now for output
  features=$(build/tools/elfz_probe "$f" || true)
  [[ -z "$features" ]] && continue

  # Progress counters
  done_now=$((ALREADY + count))
  remaining=$((TOTAL_ELF - done_now))
  echo "[RUN][$done_now/$remaining][$CODEC] $f" >&2

  rm -f /tmp/collect_data_packed 2>/dev/null || true
  bcj_log=$(run_pack bcj "$f")
  rm -f /tmp/collect_data_packed 2>/dev/null || true
  kan_log=$(run_pack kanziexe "$f")

  # Post-filter metrics
  filt_row=$(build/tools/filtered_probe --no-path "$f" 2>/dev/null || true)

  # Super-strip sizes (prefer BCJ log)
  sstrip_in=$(printf '%s\n' "$bcj_log" | get_last 's/.*Super-strip: *([0-9]+) *[^0-9]+ *([0-9]+).*/\1/p')
  sstrip_out=$(printf '%s\n' "$bcj_log" | get_last 's/.*Super-strip: *([0-9]+) *[^0-9]+ *([0-9]+).*/\2/p')
  if [[ -z "$sstrip_in" || -z "$sstrip_out" ]]; then
    sstrip_in=$(printf '%s\n' "$kan_log" | get_last 's/.*Super-strip: *([0-9]+) *[^0-9]+ *([0-9]+).*/\1/p')
    sstrip_out=$(printf '%s\n' "$kan_log" | get_last 's/.*Super-strip: *([0-9]+) *[^0-9]+ *([0-9]+).*/\2/p')
  fi
  sstrip_saved=""; [[ -n "$sstrip_in" && -n "$sstrip_out" ]] && sstrip_saved=$(( sstrip_in - sstrip_out )) || true

  # Filtered lengths (codec-agnostic)
  bcj_filtered_len=$(printf '%s\n' "$bcj_log" | get_last 's/.*Filtre BCJ[^:]*: *([0-9]+).*/\1/p')
  kanzi_filtered_len=$(printf '%s\n' "$kan_log" | get_last 's/.*Filtre Kanzi EXE[^:]*: *([0-9]+) *-> *([0-9]+).*/\2/p')

  # Stub sizes
  bcj_stub_size=$(printf '%s\n' "$bcj_log" | get_last 's/.*Stub final: *([0-9]+).*/\1/p')
  [[ -z "$bcj_stub_size" ]] && bcj_stub_size=$(printf '%s\n' "$bcj_log" | get_last 's/.*Taille stub: *([0-9]+).*/\1/p')
  kan_stub_size=$(printf '%s\n' "$kan_log" | get_last 's/.*Stub final: *([0-9]+).*/\1/p')
  [[ -z "$kan_stub_size" ]] && kan_stub_size=$(printf '%s\n' "$kan_log" | get_last 's/.*Taille stub: *([0-9]+).*/\1/p')

  # Final sizes and ratios
  bcj_final=$(printf '%s\n' "$bcj_log" | get_last 's/.*Final size: *([0-9]+).*/\1/p')
  if [[ -z "$bcj_final" && -f /tmp/collect_data_packed ]]; then bcj_final=$(stat -c %s /tmp/collect_data_packed 2>/dev/null || wc -c < /tmp/collect_data_packed); fi
  kan_final=$(printf '%s\n' "$kan_log" | get_last 's/.*Final size: *([0-9]+).*/\1/p')
  if [[ -z "$kan_final" && -f /tmp/collect_data_packed ]]; then kan_final=$(stat -c %s /tmp/collect_data_packed 2>/dev/null || wc -c < /tmp/collect_data_packed); fi
  bcj_ratio=$(printf '%s\n' "$bcj_log" | get_last 's/.*Ratio: *([0-9.]+)%.*/\1/p')
  kan_ratio=$(printf '%s\n' "$kan_log" | get_last 's/.*Ratio: *([0-9.]+)%.*/\1/p')

  # Elapsed and MAXRSS
  bcj_elapsed_s=$(printf '%s\n' "$bcj_log" | get_last 's/.*ELAPSED=([0-9.]+).*/\1/p')
  kanzi_elapsed_s=$(printf '%s\n' "$kan_log" | get_last 's/.*ELAPSED=([0-9.]+).*/\1/p')
  bcj_maxrss_kb=$(printf '%s\n' "$bcj_log" | get_last 's/.*MAXRSS=([0-9]+).*/\1/p')
  kanzi_maxrss_kb=$(printf '%s\n' "$kan_log" | get_last 's/.*MAXRSS=([0-9]+).*/\1/p')

  # Winner
  winner=""
  if [[ -n "$kan_final" && -n "$bcj_final" ]]; then
    winner="bcj"; if [[ "$kan_final" -lt "$bcj_final" ]]; then winner="kanziexe"; fi
  fi

  if [[ -z "$bcj_final" || -z "$kan_final" ]]; then
    echo "[SKIP] No final size (packer failed) for: $f" >&2
    continue
  fi

  echo "${features},${filt_row},${sstrip_in:-},${sstrip_out:-},${sstrip_saved:-},${bcj_filtered_len:-},${kanzi_filtered_len:-},${bcj_stub_size:-},${kan_stub_size:-},${bcj_final},${bcj_ratio:-},${kan_final},${kan_ratio:-},${bcj_elapsed_s:-},${kanzi_elapsed_s:-},${bcj_maxrss_kb:-},${kanzi_maxrss_kb:-},${winner:-}" >> "$OUT"

  count=$((count+1))

done

echo "Done. CSV: $OUT" >&2
