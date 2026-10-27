#!/bin/bash
# perf hardware counter measurements, run from the project folder
export OMP_PROC_BIND=true  # OS keeps switching between P and E cores, so this is for fixing the core
EVENTS=cycles,instructions,branches,branch-misses,mem_load_l3_hit_retired.xsnp_fwd
mkdir -p results/perf

for IMG in uniform peaked; do
    OMP_PLACES="{0}" perf stat -e $EVENTS -o results/perf/naive_${IMG}_T1.txt ./hist data/$IMG.raw 1 naive > results/perf/naive_${IMG}_T1_output.txt
    for M in shared binpart; do
        OMP_PLACES="{0}" perf stat -e $EVENTS -o results/perf/${M}_${IMG}_T1.txt ./hist data/$IMG.raw 1 $M > results/perf/${M}_${IMG}_T1_output.txt
        OMP_PLACES="{0},{2},{4},{6}" perf stat -e $EVENTS -o results/perf/${M}_${IMG}_T4.txt ./hist data/$IMG.raw 4 $M > results/perf/${M}_${IMG}_T4_output.txt
    done
done

echo "done: $(ls results/perf | wc -l) files"

for f in results/perf/*_T?.txt; do echo "=== $f"; grep -E "cpu_core|elapsed" $f; done | tee results/perf_summary.txt