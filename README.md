# Optimising Multithreaded Histogram Computation on a Multi-core CPU

CS683 (Advanced Computer Architecture) Course project

## Goal
Build an image histogram (for this project I am taking it to be just the count of pixels per intensity value, 256 bins) in parallel
using several methods, measure how each interacts with the CPU hardware using
`perf` hardware counters, and explain which is fastest and why.

## Methods
| # | Method | What it exposes |
|---|--------|-----------------|
| 0 | Naive (single thread) | Correct reference answer |
| 1 | Shared histogram | Contention on shared counters |
| 2 | Bin-partitioned | Redundant image reads, branch mispredictions |
| 3 | Privatized, packed | False sharing |
| 4 | Privatized, padded | Hopefully the fix (cache-line aligned) |

## Status
- [x] Test image generation (uniform + peaked)
- [x] Method 0: naive reference, verified
- [x] Method 1: shared (racy just to show the contention leading to wrong work, and then the atomised version)
- [x] Method 2: bin-partitioned
- [x] Speedup v/s Threads and perf hardware counter plots
- [ ] Methods 3-4 (Checkpoint II)

## Test images
4096*4096 pixels, 8-bit grayscale, raw bytes (16 MB each, larger than the 12 MB L3 cache).
- `uniform.raw`: every value 0-255 equally likely
- `peaked.raw`: Gaussian dist, mean 128, std dev 3 (almost all pixels in bins ~119-137, the mean+-3std)

Generated with a fixed seed (69), so they are identical on every run (not committed to git).

## Build
```bash
mkdir -p data
g++ -O2 -fopenmp -g -Wall -o gen gen.cpp
g++ -O2 -fopenmp -g -Wall -o hist hist.cpp
./gen
```

## Run
```bash
./hist <image> <threads> <method>     # image needs the address of the image histograms are being run on, threads is the number of threads methods 1 and 2 will use and method selection is from the list ["naive", "racy", "shared", "binpart", "all" (default)]

# example of a single method run on 4 threads
# I suggest you to also do OMP_PROC_BIND=true if you have different types of cores (performance and efficiency) as OS can run the thread on different types for different times leading to noisy runs.
OMP_PROC_BIND=true OMP_PLACES="{0},{2},{4},{6}" ./hist data/peaked.raw 4 shared
```

## Reproduce my results
```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor   # please keep your charger plugged in, I beleive laptops tend to put limits on power consumption even when in performance mode
bash run_scaling.sh     # results/scaling.csv
bash run_perf.sh        # results/perf/, results/perf_summary.txt
```
In my run threads are pinned to the separate physical P-cores (CPUs: 0, 2, 4 ,6. I have avoided the SMT for now). And in 8 thread run I have taken all of my physical cores.

## Results so far (4 threads)
| Method | Uniform | Peaked | Main cause (perf) |
|--------|---------|--------|-------------------|
| Naive (1 thread) | 5.8 ms | 6.0 ms | - |
| Shared (atomised) | 130 ms | 165 ms | ~100M loads served from another core's cache, IPC 0.06-0.08 |
| Bin-partitioned | 82 ms | 58 ms | branch misses, over 3x more instructions |

Both parallel methods are slower than the naive single threaded one. The racy version loses counts, which can se been in results.

## Hardware used by me 
Intel Core i5-12450H (4 P-cores + 4 E-cores, 12 threads), 12 MB shared L3.
Full details in `results/env/`.
