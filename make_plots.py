import numpy as np
import matplotlib.pyplot as plt
import csv
import os
import re


os.makedirs('figures', exist_ok=True)
IMAGES = ['uniform', 'peaked']


# MAKING THE ACTUAL HISTOGRAMS
fig, subfigs = plt.subplots(1, 2, figsize=(10, 5))

for h, name in zip(subfigs, IMAGES):
    img = np.fromfile(f"data/{name}.raw", dtype=np.uint8)
    histogram = np.bincount(img, minlength=256)

    h.bar(range(256), 100*histogram/img.size, width=1)
    h.set_title(f"{name} image (4096 x 4096)")

    h.set_xlabel("pixel value = bin")
    h.set_ylabel("% of pixels in each bin")

fig.tight_layout()
fig.savefig("figures/1_histograms.png")





# SPEEDUP VS THE NUMBER OF THREADS
# speedup = t_naive / t_method

csv_data= list(csv.DictReader(open("results/scaling.csv")))
fig, subfigs = plt.subplots(1, 2, figsize=(10, 5), sharey=True)

for subfig, name in zip(subfigs, IMAGES):
    relevant_rows = [x for x in csv_data if name in x["image"]]

    naive = np.mean([float(x['median_ms']) for x in relevant_rows if x['method']=='naive'])

    for method in ['shared', 'binpart']: #left racy cuz it gave wrong answer
        points = sorted((int(x["threads"]), float(x["median_ms"])) for x in relevant_rows if x["method"] == method)
        T = [p[0] for p in points] #threads
        S = [naive/p[1] for p in points] #speedup
        
        subfig.plot(T, S, label=method)
    subfig.set_ylim(0, 1.2)
    subfig.set_xscale("log")
    subfig.set_xticks([1, 2, 4, 8], ["1P", "2P", "4P", "4P+4E"])
    subfig.set_title(f"{name} image (naive = {naive:.1f} ms)")
    subfig.set_xlabel("threads")
    subfig.legend()

subfigs[0].set_ylabel("speedup compared to naive")
fig.tight_layout()
fig.savefig("figures/2_speedup.png")


# PERF STATS

def read_perf(path):
    '''return {event:count} from perf stats'''
    numbers = {}
    for line in open(path):
        match = re.match(r"\s*([\d,]+)\s+cpu_core/([\w.\-]+)/", line)
        if match:
            count = int(match.group(1).replace(",", ""))
            name = match.group(2)
            numbers[name] = count
    return numbers


runs = [("naive", 1), ("shared", 1), ("shared", 4), ("binpart", 1), ("binpart", 4)]
run_names = [f"{m}\n{t}T" for m, t in runs]

ipc_uniform,  ipc_peaked  = [], []
miss_uniform, miss_peaked = [], []
xsnp_uniform, xsnp_peaked = [], []

table_file = open("results/perf_table.csv", "w")
table_file.write("image,method,threads,IPC,branch_miss_percent,xsnp_fwd\n")

for image in IMAGES:
    for method, threads in runs:
        p = read_perf(f"results/perf/{method}_{image}_T{threads}.txt")

        ipc  = p["instructions"] / p["cycles"]
        miss = 100 * p["branch-misses"] / p["branches"]
        xsnp = p["mem_load_l3_hit_retired.xsnp_fwd"]

        if image == "uniform":
            ipc_uniform.append(ipc)
            miss_uniform.append(miss)
            xsnp_uniform.append(xsnp)
        else:
            ipc_peaked.append(ipc)
            miss_peaked.append(miss)
            xsnp_peaked.append(xsnp)

        table_file.write(f"{image},{method},{threads},{ipc:.3f},{miss:.3f},{xsnp}\n")
        print(f"{image:8} {method:8} {threads}T   IPC={ipc:.2f}   branch-miss={miss:.2f}%   xsnp_fwd={xsnp:,}")

table_file.close()


fig, axes = plt.subplots(1, 3, figsize=(15, 5))


centers = [0, 1, 2, 3, 4]
bar_width = 0.4
uniform_x = [c - 0.2 for c in centers]   # uniform LEFT 
peaked_x  = [c + 0.2 for c in centers]   # peaked RIGHT

#IPC
ax = axes[0]
ax.bar(uniform_x, ipc_uniform, width=bar_width, label="uniform")
ax.bar(peaked_x,  ipc_peaked,  width=bar_width, label="peaked")
ax.set_xticks(centers)
ax.set_xticklabels(run_names)
ax.set_title("IPC (instructions per cycle)\nhigher = busy, lower = waiting")
ax.legend()

# branch miss rate
ax = axes[1]
ax.bar(uniform_x, miss_uniform, width=bar_width, label="uniform")
ax.bar(peaked_x,  miss_peaked,  width=bar_width, label="peaked")
ax.set_xticks(centers)
ax.set_xticklabels(run_names)
ax.set_title("Branch misses\n(% of branches guessed wrong)")
ax.legend()

# cross-core cache transfers (log scale because values range from ~1 thousand to ~100 million)
ax = axes[2]
ax.bar(uniform_x, xsnp_uniform, width=bar_width, label="uniform")
ax.bar(peaked_x,  xsnp_peaked,  width=bar_width, label="peaked")
ax.set_xticks(centers)
ax.set_xticklabels(run_names)
ax.set_yscale("log")
ax.set_title("Loads served from ANOTHER core's cache\n(xsnp_fwd, log scale)")
ax.legend()

fig.tight_layout()
fig.savefig("figures/3_perf_counters.png")
print("Saved figues and csv")