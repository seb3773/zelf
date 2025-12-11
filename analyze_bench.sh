#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

CSV1="bench_lzav_auto.csv"      # predictor = lz4
CSV2="bench_lzav_auto_HC.csv"   # predictor = lz4hc
CSV3="bench_lzav_auto_rule.csv" # updated rule (auto)

summary() {
  local csv="$1"
  if [[ ! -f "$csv" ]]; then
    echo "Skipping $csv (not found)." >&2
    return
  fi
  echo "== SUMMARY: $csv =="
  awk -F, '{
    n++; ok+=($14+0)
    pred=($8=="kanzi"?$11:$12); best=($11<$12?$11:$12)
    sumpen+=pred-best
    d=($11-$12); if (d<0) d=-d; sumabs+=d
    win_k+=($11<$12); win_b+=($12<=$11)
  } END {
    printf("N=%d acc=%.2f%% avg_penalty=%.1fB mean_abs_pair_delta=%.1fB real: Kanzi=%d (%.1f%%) BCJ=%d (%.1f%%)\n",
           n, 100*ok/n, sumpen/n, sumabs/n, win_k, 100*win_k/n, win_b, 100*win_b/n)
  }' "$csv"
}

confusion() {
  local csv="$1"
  echo "== CONFUSION: $csv =="
  awk -F, '{ M[$8 "," $13]++ } END {
    printf("pred:kanzi→real:kanzi=%d  pred:kanzi→real:bcj=%d  pred:bcj→real:kanzi=%d  pred:bcj→real:bcj=%d\n",
      M["kanzi,kanzi"]+0, M["kanzi,bcj"]+0, M["bcj,kanzi"]+0, M["bcj,bcj"]+0)
  }' "$csv"
}

buckets_size() {
  local csv="$1"
  echo "== SIZE BUCKETS: $1 =="
  awk -F, '{
    s=$2+0; b=(s<131072)?"small":(s<1048576?"mid":"large");
    n[b]++; ok[b]+=($14+0)
  } END { for (b in n) printf("%s acc=%.2f%% N=%d\n", b, 100*ok[b]/n[b], n[b]) }' "$csv"
}

buckets_kexp() {
  local csv="$1"
  echo "== KANZI EXPANSION BUCKETS: $1 =="
  awk -F, '{
    kx=$15+0.0; b="<=0.2%";
    if (kx>0.2) b="0.2-2%";
    if (kx>2.0) b=">2%";
    n[b]++; ok[b]+=($14+0)
  } END { for (b in n) printf("%s acc=%.1f%% N=%d\n", b, 100*ok[b]/n[b], n[b]) }' "$csv"
}

top_mismatches() {
  local csv="$1"
  echo "== TOP 20 MISMATCH PENALTIES: $1 =="
  awk -F, '$14==0{
    pred=($8=="kanzi"?$11:$12); best=($11<$12?$11:$12); pen=pred-best;
    print pen, $1
  }' "$csv" | sort -nr | head -20
}

compare_predictors() {
  local csv1="$1" csv2="$2"
  echo "== PREDICTOR DISAGREEMENTS ($csv1 vs $csv2) =="
  awk -F, 'FNR==NR {dec1[$1]=$8; real[$1]=$13; k[$1]=$11; b[$1]=$12; next}
           {dec2=$8; p=$1; if (p in dec1 && dec1[p]!=dec2) {
              pen1=((dec1[p]=="kanzi")?k[p]:b[p]) - (k[p]<b[p]?k[p]:b[p]);
              pen2=((dec2=="kanzi")?k[p]:b[p]) - (k[p]<b[p]?k[p]:b[p]);
              print p, "pred1="dec1[p], "pred2="dec2, "real="real[p], "pen1="pen1, "pen2="pen2
           }}' "$csv1" "$csv2" | head -50
}

sweep_thresholds() {
  local csv="$1" label="$2"
  echo "== THRESHOLD SWEEP on $csv ($label) =="
  # GE in % (guard expansion), TE in % (tie window)
  for GE in 0.5 1.0 1.5 2.0 2.5 3.0; do
    for TE in 0.3 0.5 0.7 1.0; do
      awk -F, -v GE="$GE" -v TE="$TE" '{
        sz=$2+0; pk=$6+0; pb=$7+0; kexp=$15+0.0;
        if (kexp > GE) dec="bcj";
        else {
          rel=(pk-pb)/sz;
          if (rel <= -TE/100.0) dec="kanzi";
          else if (rel >= TE/100.0) dec="bcj";
          else dec=(kexp <= 0.2 ? "kanzi" : "bcj");
        }
        okc += (dec==$13);
        best = ($11<$12?$11:$12);
        pred = (dec=="kanzi"?$11:$12);
        pen += pred - best;
        n++;
      } END {
        printf("GE=%s TE=%s  acc=%.2f%%  avg_penalty=%.1fB (N=%d)\n", GE, TE, 100*okc/n, pen/n, n)
      }' "$csv"
    done
  done
}

main() {
  any=0
  if [[ -f "$CSV1" ]]; then
    summary "$CSV1"; echo
    confusion "$CSV1"; echo
    buckets_size "$CSV1"; echo
    buckets_kexp "$CSV1"; echo
    top_mismatches "$CSV1"; echo
    sweep_thresholds "$CSV1" "pred=LZ4"; echo
    any=1
  fi
  if [[ -f "$CSV2" ]]; then
    summary "$CSV2"; echo
    confusion "$CSV2"; echo
    buckets_size "$CSV2"; echo
    buckets_kexp "$CSV2"; echo
    top_mismatches "$CSV2"; echo
    sweep_thresholds "$CSV2" "pred=LZ4HC"; echo
    any=1
  fi
  if [[ -f "$CSV1" && -f "$CSV2" ]]; then
    compare_predictors "$CSV1" "$CSV2"; echo
  fi
  if [[ -f "$CSV3" ]]; then
    echo
    echo "==== ANALYSIS: $CSV3 (updated rule) ===="
    summary "$CSV3"; echo
    confusion "$CSV3"; echo
    buckets_size "$CSV3"; echo
    buckets_kexp "$CSV3"; echo
    top_mismatches "$CSV3"; echo
    any=1
  fi
  if [[ "$any" -eq 0 ]]; then
    echo "No bench CSVs found (expected $CSV1, $CSV2, or $CSV3)." >&2
    exit 1
  fi
}

main "$@"
