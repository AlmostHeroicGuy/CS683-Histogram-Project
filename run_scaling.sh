#!/bin/bash
# timing sweep over thread counts for plotting, output in in results/scaling.csv
export OMP_PROC_BIND=true   # same as perf script
OUT=results/scaling.csv
echo "tag,method,image,threads,median_ms,mean_ms" > $OUT

for IMG in data/uniform.raw data/peaked.raw; do
    for T in 1 2 4 8; do
        case $T in
            1) x="{0}";;
            2) x="{0},{2}";;
            4) x="{0},{2},{4},{6}";;
            8) x="{0},{2},{4},{6},{8},{9},{10},{11}";;   # using the E cores as well (cpu_atom)
        esac
        OMP_PLACES="$x" ./hist $IMG $T all | grep CSV >> $OUT
    done
done
cat $OUT