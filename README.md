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
| 2 | Bin-partitioned | Redundant image reads, load imbalance |
| 3 | Privatized, packed | False sharing |
| 4 | Privatized, padded | The fix (cache-line aligned) |

## Status
- [x] Test image generator (uniform + peaked)
- [x] Method 0: naive reference, verified
- [ ] Method 1: shared
- [ ] Method 2: bin-partitioned
- [ ] Methods 3–4 (Checkpoint II)

## Test images
4096 × 4096 pixels, 8-bit grayscale, raw bytes (16 MB each, larger than the 12 MB L3 cache).
- `uniform.raw`: every value 0–255 equally likely
- `peaked.raw`: Gaussian, mean 128, std dev 3 (almost all pixels in bins ~119–137, the mean+-3std)

Generated with a fixed seed (69), so they are identical on every run (not committed to git).

## Build and run
```bash
mkdir -p data
g++ -O2 -fopenmp -g -Wall -o gen gen.cpp
g++ -O2 -fopenmp -g -Wall -o hist hist.cpp
./gen
./hist data/peaked.raw
```

## Hardware used by me 
Intel Core i5-12450H (4 P-cores + 4 E-cores, 12 threads), 12 MB shared L3.
Full details in `results/env/`.
