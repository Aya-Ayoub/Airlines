#!/usr/bin/env bash
# Usage: ./run_bench.sh [DATA_CSV] [REF_CSV]
#   DATA_CSV  input file to time (default data/airline_clean.csv, the full 639 MB file)
#   REF_CSV   known-good sequential result to compare against (default results/seq_100MB.csv).
#             Only meaningful if DATA_CSV is the same 100MB input Person A used.
# Runs seq + every parallel version at 1,2,4,8 threads, 3 runs each, checks every run against
# the sequential output, and writes results/bench_raw.csv. Then run: python3 analyze.py
set -uo pipefail
cd "$(dirname "$0")"

DATA=${1:-data/airline_clean.csv}
REF=${2:-results/seq_100MB.csv}
RUNS=3
THREADS="1 2 4 8"
SCHEDS="static dynamic:1 dynamic:4096 guided"   # label uses : (a comma would break the CSV)
RAW=results/bench_raw.csv
C=data/carriers.txt; O=data/origins.txt

[ -f "$DATA" ] || { echo "missing $DATA"; exit 1; }
make -s || exit 1
mkdir -p results plots

# ---- ground truth = sequential run on the same input
./build/seq "$DATA" $C $O results/ref_seq.csv > results/ref_seq.out
REF_CHECK=$(grep '^CHECK' results/ref_seq.out)
echo "sequential: $REF_CHECK"
if [ -f "$REF" ]; then
  if cmp -s results/ref_seq.csv "$REF"; then echo "matches $REF  (good)"; else echo "NOTE: differs from $REF (expected unless DATA is the same 100MB input)"; fi
fi

echo "variant,schedule,threads,run,load,compute,total,ok" > $RAW
BAD=0

one() {  # variant schedule threads run
  local v=$1 s=$2 t=$3 r=$4 sched=$2
  [ "$s" = "-" ] && sched=static
  OMP_SCHEDULE=${sched/:/,} ./build/$v "$DATA" $C $O results/run.csv $t > results/run.out
  local ok=1
  cmp -s results/run.csv results/ref_seq.csv || ok=0
  [ "$(grep '^CHECK' results/run.out)" = "$REF_CHECK" ] || ok=0
  [ $ok = 1 ] || { BAD=1; echo "!!! MISMATCH: $v $s t=$t run=$r"; }
  local L=$(awk '/^TIME_LOAD/{print $2}' results/run.out)
  local P=$(awk '/^TIME_COMPUTE/{print $2}' results/run.out)
  local T=$(awk '/^TIME_TOTAL/{print $2}' results/run.out)
  echo "$v,$s,$t,$r,$L,$P,$T,$ok" >> $RAW
  printf "%-14s %-13s t=%d run=%d  compute=%s total=%s %s\n" $v $s $t $r $P $T $([ $ok = 1 ] && echo OK || echo FAIL)
}

for r in $(seq $RUNS); do one seq - 1 $r; done
for t in $THREADS; do
  for r in $(seq $RUNS); do
    for s in $SCHEDS; do one omp_reduction $s $t $r; done
    one omp_atomic    - $t $r
    one omp_critical  - $t $r
    one std_thread    - $t $r
  done
done

[ $BAD = 0 ] && echo "ALL RUNS MATCH THE SEQUENTIAL RESULT" || echo "SOME RUNS DID NOT MATCH - see !!! lines"
echo "raw timings: $RAW   next: python3 analyze.py"