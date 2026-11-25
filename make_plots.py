import numpy as np
import matplotlib.pyplot as plt
import csv
import os
import re


# figures folder, make it if we dont already have one
if os.path.isdir("figures") == False:
    os.makedirs("figures")

# the Two images. both are 4096 x 4096 raw bytes, no header
IMAGES = ["uniform", "peaked"]


# =========================
# MAKING THE ACTUAL HISTOGRAMS
# =========================
fig, subfigs = plt.subplots(1, 2, figsize=(10, 5))

for i in range(len(IMAGES)):
    name = IMAGES[i]
    h = subfigs[i]

    # read the whole file, one byte = one pixel
    filename = "data/" + name + ".raw"
    img = np.fromfile(filename, dtype=np.uint8)

    # how many pixels landed in each value from 0 to 255
    histogram = np.bincount(img, minlength=256)

    total_pixels = img.size
    percents = []
    for count in histogram:
        # percent not raw count, otherwise the y axis is just a huge number
        pct = 100.0 * float(count) / float(total_pixels)
        percents.append(pct)

    bins = []
    for b in range(256):
        bins.append(b)

    h.bar(bins, percents, width=1)
    h.set_title(name + " image (4096 x 4096)")
    h.set_xlabel("pixel value = bin")
    h.set_ylabel("% of pixels in each bin")

fig.tight_layout()
fig.savefig("figures/1_histograms.png")


# ============================================
# SPEEDUP VS THE NUMBER OF THREADS
# speedup = t_naive / t_method   (Higher = Better)
# ============================================

# pull every row out of the csv. DictReader is fine, the file isnt that big
csv_file = open("results/scaling.csv")
reader = csv.DictReader(csv_file)
csv_data = []
for row in reader:
    csv_data.append(row)
csv_file.close()

fig, subfigs = plt.subplots(1, 2, figsize=(10, 5), sharey=True)

for i in range(len(IMAGES)):
    name = IMAGES[i]
    subfig = subfigs[i]

    # only the rows that belong to this image
    relevant_rows = []
    for row in csv_data:
        image_name = row["image"]
        if name in image_name:
            relevant_rows.append(row)

    # naive time. theres usually just one but we average anyway
    naive_times = []
    for row in relevant_rows:
        if row["method"] == "naive":
            ms = float(row["median_ms"])
            naive_times.append(ms)

    naive_sum = 0.0
    for ms in naive_times:
        naive_sum = naive_sum + ms
    naive = naive_sum / len(naive_times)

    # left racy out cuz it gave the wrong Answer. not debugging that again
    methods = ["shared", "binpart", "packed", "private"]

    for method in methods:
        # (thread count, median ms) then sort so the line doesnt zig zag
        points = []
        for row in relevant_rows:
            if row["method"] == method:
                thread_count = int(row["threads"])
                median_ms = float(row["median_ms"])
                points.append((thread_count, median_ms))

        points = sorted(points)

        T = []   # threads
        S = []   # speedup
        for p in points:
            thread_count = p[0]
            median_ms = p[1]
            speedup = naive / median_ms
            T.append(thread_count)
            S.append(speedup)

        subfig.plot(T, S, "o-", label=method)

        # only write the number on private, otherwise the labels pile up
        if method == "private":
            for j in range(len(T)):
                t = T[j]
                s = S[j]
                label = "{:.1f}x".format(s)
                subfig.annotate(label, (t, s), textcoords="offset points", xytext=(0, 8), ha="center")

    # dashed line where speedup = 1, ie same speed as naive
    subfig.axhline(1, color="gray", linestyle="--", linewidth=1)
    subfig.set_yscale("log")
    subfig.set_xscale("log", base=2)
    subfig.minorticks_off()

    # took a while to get these tick label right. 8 isnt 8P, its 4P + 4E
    subfig.set_yticks([0.03, 0.1, 0.3, 1, 2, 4, 8], ["0.03", "0.1", "0.3", "1", "2", "4", "8"])
    subfig.set_xticks([1, 2, 4, 8], ["1P", "2P", "4P", "4P+4E"])

    title = name + " image (naive = " + "{:.1f}".format(naive) + " ms)"
    subfig.set_title(title)
    subfig.set_xlabel("threads")
    subfig.legend()

subfigs[0].set_ylabel("speedup compared to naive (higher is better)")
fig.tight_layout()
fig.savefig("figures/2_speedup.png")


# ============================================
# SPEEDUP AT 4 THREADS, ONE BAR PER METHOD
# same numbers as above, just easier to read at a glance
# ============================================
fig, subfigs = plt.subplots(1, 2, figsize=(10, 5), sharey=True)

for i in range(len(IMAGES)):
    name = IMAGES[i]
    subfig = subfigs[i]

    relevant_rows = []
    for row in csv_data:
        if name in row["image"]:
            relevant_rows.append(row)

    naive_times = []
    for row in relevant_rows:
        if row["method"] == "naive":
            naive_times.append(float(row["median_ms"]))

    naive_sum = 0.0
    for ms in naive_times:
        naive_sum = naive_sum + ms
    naive = naive_sum / float(len(naive_times))

    methods = ["shared", "binpart", "packed", "private"]
    speedups = []

    for method in methods:
        # grab the 4 thread run. should be exactly one
        t4 = None
        for row in relevant_rows:
            same_method = row["method"] == method
            is_four = row["threads"] == "4"
            if same_method and is_four:
                t4 = float(row["median_ms"])
                break

        this_speedup = naive / t4
        speedups.append(this_speedup)

    subfig.bar(methods, speedups)

    # number on top of the bar so you dont have to squint at the log axis
    for position in range(len(speedups)):
        s = speedups[position]
        txt = "{:.2f}x".format(s)
        subfig.text(position, s * 1.15, txt, ha="center")

    subfig.axhline(1, color="gray", linestyle="--", linewidth=1)
    subfig.set_yscale("log")
    subfig.minorticks_off()
    subfig.set_yticks([0.01, 0.1, 1, 10], ["0.01", "0.1", "1", "10"])

    title = name + " image, 4 threads (naive = " + "{:.1f}".format(naive) + " ms)"
    subfig.set_title(title)

subfigs[0].set_ylabel("speedup over naive (higher is better, log scale)")
fig.tight_layout()
fig.savefig("figures/4_speedup_4threads.png")


# ===========
# PERF STATS
# ===========

def read_perf(path):
    '''return {event name: count} from a perf stat text file
    the spacing in these dumps is kinda inconsistant so just regex it'''
    numbers = {}

    f = open(path)
    lines = f.readlines()
    f.close()

    for line in lines:
        # example: 1,234,567 cpu_core/instructions/
        match = re.match(r"\s*([\d,]+)\s+cpu_core/([\w.\-]+)/", line)
        if match == None:
            continue

        count_txt = match.group(1)
        count_txt = count_txt.replace(",", "")
        count = int(count_txt)

        event_name = match.group(2)
        numbers[event_name] = count

    return numbers


# which runs we actually plotted. order here is the order of the bars
runs = []
runs.append(("naive", 1))
runs.append(("shared", 1))
runs.append(("shared", 4))
runs.append(("binpart", 1))
runs.append(("binpart", 4))
runs.append(("packed", 1))
runs.append(("packed", 4))
runs.append(("private", 1))
runs.append(("private", 4))

run_names = []
for pair in runs:
    method = pair[0]
    threads = pair[1]
    label = method + "\n" + str(threads) + "T"
    run_names.append(label)

ipc_uniform = []
ipc_peaked = []
miss_uniform = []
miss_peaked = []
xsnp_uniform = []
xsnp_peaked = []

table_file = open("results/perf_table.csv", "w")
table_file.write("image,method,threads,IPC,branch_miss_percent,xsnp_fwd\n")

for image in IMAGES:
    for pair in runs:
        method = pair[0]
        threads = pair[1]

        perf_path = "results/perf/" + method + "_" + image + "_T" + str(threads) + ".txt"
        p = read_perf(perf_path)

        # ipc = how much real work per cycle. low means the core is waiting on Something
        instructions = p["instructions"]
        cycles = p["cycles"]
        ipc = float(instructions) / float(cycles)

        # percent of branches the predictor got wrong
        branch_misses = p["branch-misses"]
        branches = p["branches"]
        miss = 100.0 * float(branch_misses) / float(branches)

        # loads that got the line from ANOTHER cores cache. this one gets stupidly big
        xsnp = p["mem_load_l3_hit_retired.xsnp_fwd"]

        if image == "uniform":
            ipc_uniform.append(ipc)
            miss_uniform.append(miss)
            xsnp_uniform.append(xsnp)
        else:
            ipc_peaked.append(ipc)
            miss_peaked.append(miss)
            xsnp_peaked.append(xsnp)

        ipc_txt = "{:.3f}".format(ipc)
        miss_txt = "{:.3f}".format(miss)
        line = image + "," + method + "," + str(threads) + "," + ipc_txt + "," + miss_txt + "," + str(xsnp) + "\n"
        table_file.write(line)

        # keep the print the same so the terminal dump still lines up
        print("%-8s %-8s %dT   IPC=%.2f   branch-miss=%.2f%%   xsnp_fwd=%s" % (
            image, method, threads, ipc, miss, format(xsnp, ",")
        ))

table_file.close()


fig, axes = plt.subplots(1, 3, figsize=(22, 5))

# one slot per run, then two skinny bars inside it
centers = []
for i in range(len(runs)):
    centers.append(i)

bar_width = 0.4

uniform_x = []   # uniform goes on the Left of each pair
peaked_x = []    # peaked on the Right
for c in centers:
    left = c - 0.2
    right = c + 0.2
    uniform_x.append(left)
    peaked_x.append(right)


# IPC
ax = axes[0]
ax.bar(uniform_x, ipc_uniform, width=bar_width, label="uniform")
ax.bar(peaked_x, ipc_peaked, width=bar_width, label="peaked")
ax.set_xticks(centers)
ax.set_xticklabels(run_names)
ax.set_title("IPC (instructions per cycle)\nhigher = busy, lower = waiting")
ax.legend()

# branch miss rate
ax = axes[1]
ax.bar(uniform_x, miss_uniform, width=bar_width, label="uniform")
ax.bar(peaked_x, miss_peaked, width=bar_width, label="peaked")
ax.set_xticks(centers)
ax.set_xticklabels(run_names)
ax.set_title("Branch misses\n(% of branches guessed wrong, lower is better)")
ax.legend()

# cross core cache hits. log because its like 1k vs 100M
ax = axes[2]
ax.bar(uniform_x, xsnp_uniform, width=bar_width, label="uniform")
ax.bar(peaked_x, xsnp_peaked, width=bar_width, label="peaked")
ax.set_xticks(centers)
ax.set_xticklabels(run_names)
ax.set_yscale("log")
ax.set_title("Loads served from ANOTHER core's cache\n(xsnp_fwd, log scale, lower is better)")
ax.legend()

fig.tight_layout()
fig.savefig("figures/3_perf_counters.png")
print("Saved figues and csv")
