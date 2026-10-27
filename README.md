# Optimising Multithreaded Histogram Computation on a Multi-core CPU

CS683 (Advanced Computer Architecture) course project.

For this project I am taking a histogram to be just the count of pixels per intensity value (256 bins for an 8-bit image). The idea is to build it in parallel using a few different methods and then measure how each one interacts with the CPU hardware using `perf` hardware counters, and explain which is fastest and why.

So far I have done three methods. Method 0 is a plain single-threaded histogram, which I use as the correct reference answer. Method 1 is a shared histogram where all threads update the same 256 counters. I first wrote it without any protection (the "racy" version) just to show the race condition, where it loses counts, and then fixed it with an atomic increment. Method 2 is bin-partitioned: each thread owns a range of bins and
scans the whole image, counting only its own bins into a private array. For Checkpoint 2 I plan to do privatized histograms per thread, first packed (to show false sharing) and then padded to cache-line size, which is hopefully the fix.

The test images are 4096x4096, 8-bit grayscale, stored as raw bytes (16 MB each, which is larger than the 12 MB L3 cache). `uniform.raw` has every value 0-255 equally likely, and `peaked.raw` is a Gaussian with mean 128 and std dev 3, so almost all pixels land in bins 119-137 (mean +- 3*std). They are generated with a fixed seed (69) so they are identical on every run, and are not committed to git. I have taken these 2 synthetic images because I wanted to see if methods vary in different image distributions and normal images wouldn't have given this control.

To build and generate the images:

```bash
mkdir -p data
g++ -O2 -fopenmp -g -Wall -o gen gen.cpp
g++ -O2 -fopenmp -g -Wall -o hist hist.cpp
./gen
```

To run: `./hist <image> <threads> <method>`, where method is one of naive, racy, shared, binpart or all (default). If your CPU has performance and efficiency cores, I suggest also setting `OMP_PROC_BIND=true` with `OMP_PLACES`, because otherwise the OS can move threads between core types and the runs get noisy. For example:

```bash
OMP_PROC_BIND=true OMP_PLACES="{0},{2},{4},{6}" ./hist data/peaked.raw 4 shared
```

To reproduce my results, set the performance governor and keep the charger plugged in (I believe laptops limit power even in performance mode), then run the two scripts and the plotting script:

```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
bash run_scaling.sh     #  results/scaling.csv
bash run_perf.sh        # results/perf/, results/perf_summary.txt
python make_plots.py    # figures/ and results/perf_table.csv
```

In my runs the threads are pinned to separate physical P-cores (CPUs 0, 2, 4, 6; I have avoided SMT for now), and the 8-thread run uses all of my physical cores (4 P + 4 E).

What I found so far, at 4 threads: 
The naive single-threaded version takes about 5.8 ms (uniform) and 6.0 ms (peaked). 
The atomic shared version takes 130 ms and 165 ms, and perf shows around 100 million loads being served from another core's cache, with IPC dropping to 0.06-0.08, so the cores are mostly waiting for the cache line to bounce between them. 
The bin-partitioned version takes 82 ms and 58 ms; there is no cross-core traffic, but it runs over 3x more instructions (every thread scans the whole image) and has a lot of branch misses (19% on uniform, 9% on peaked). So both parallel methods are currently slower than the single-threaded one. 
The racy version gives wrong answers and loses up to about half the counts. All the raw numbers are in `results/` and the plots in `figures/`.

Hardware used: Intel Core i5-12450H (4 P-cores + 4 E-cores, 12 threads), 12 MB shared L3.
Full details are in `results/env/`.
