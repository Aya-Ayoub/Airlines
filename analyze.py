#!/usr/bin/env python3
"""Reads results/bench_raw.csv (from run_bench.sh) and produces:
  results/summary.csv      median time, speedup S, efficiency E, serial fraction f (Karp-Flatt)
  plots/speedup.png        speedup vs threads  (left: compute only, right: end-to-end incl. serial load)
  plots/efficiency.png     efficiency vs threads
  plots/schedules.png      schedule comparison (static / dynamic / guided)
and prints the tables to paste into the report.

Definitions (T_seq = median of the sequential baseline, T_p = median with p threads):
  S = T_seq / T_p        E = S / p        f = (1/S - 1/p) / (1 - 1/p)   (Karp-Flatt, p > 1)
"""
import csv, statistics as st, sys
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RAW = "results/bench_raw.csv"
rows = list(csv.DictReader(open(RAW)))
if any(r["ok"] != "1" for r in rows):
    print("WARNING: some runs did not match the sequential output - fix that before using these numbers!\n")

data = defaultdict(lambda: defaultdict(list))      # (variant, sched) -> threads -> [(load, compute, total)]
for r in rows:
    data[(r["variant"], r["schedule"])][int(r["threads"])].append(
        (float(r["load"]), float(r["compute"]), float(r["total"])))

def med(key, p, col):  # col 0=load 1=compute 2=total
    return st.median(x[col] for x in data[key][p])

seq = ("seq", "-")
T_seq_c, T_seq_t, T_seq_l = med(seq, 1, 1), med(seq, 1, 2), med(seq, 1, 0)
threads = sorted({int(r["threads"]) for r in rows if r["variant"] != "seq"})
pmax = max(threads)

def karp_flatt(S, p):
    return float("nan") if p == 1 else (1 / S - 1 / p) / (1 - 1 / p)

# best schedule = lowest median compute time at the largest thread count
sched_keys = [k for k in data if k[0] == "omp_reduction"]
best = min(sched_keys, key=lambda k: med(k, pmax, 1))
main = [("OpenMP reduction (" + best[1] + ")", best),
        ("OpenMP atomic", ("omp_atomic", "-")),
        ("OpenMP critical", ("omp_critical", "-")),
        ("std::thread + atomic", ("std_thread", "-"))]

print(f"Sequential baseline: load {T_seq_l:.3f}s  compute {T_seq_c:.3f}s  total {T_seq_t:.3f}s")
print(f"Serial load fraction of sequential total: {T_seq_l / T_seq_t:.1%}\n")

out = open("results/summary.csv", "w", newline="")
w = csv.writer(out)
w.writerow(["variant", "schedule", "threads", "load_s", "compute_s", "total_s",
            "S_compute", "E_compute", "f_compute", "S_total", "E_total", "f_total"])
for key in sorted(data):
    for p in threads:
        if p not in data[key]: continue
        L, P, T = med(key, p, 0), med(key, p, 1), med(key, p, 2)
        Sc, St = T_seq_c / P, T_seq_t / T
        w.writerow([key[0], key[1], p, f"{L:.3f}", f"{P:.3f}", f"{T:.3f}",
                    f"{Sc:.2f}", f"{Sc / p:.2f}", f"{karp_flatt(Sc, p):.3f}",
                    f"{St:.2f}", f"{St / p:.2f}", f"{karp_flatt(St, p):.3f}"])
out.close()

def table(title, key, col, T_seq):
    print(title)
    print(f"{'threads':>7} {'time(s)':>8} {'S':>6} {'E':>6} {'f':>7}")
    print(f"{'seq':>7} {T_seq:8.3f} {1:6.2f} {1:6.2f} {'-':>7}")
    for p in threads:
        t = med(key, p, col); S = T_seq / t
        f = karp_flatt(S, p)
        print(f"{p:7d} {t:8.3f} {S:6.2f} {S / p:6.2f} {f:7.3f}")
    print()

table(f"== Main version: OpenMP reduction, schedule={best[1]} -- COMPUTE phase only ==", best, 1, T_seq_c)
table(f"== Main version: OpenMP reduction, schedule={best[1]} -- END-TO-END (load + compute) ==", best, 2, T_seq_t)

print("== Compute time (s) by version ==")
print(f"{'version':<28}" + "".join(f"{p:>9d}" for p in threads))
for name, key in main:
    print(f"{name:<28}" + "".join(f"{med(key, p, 1):9.3f}" for p in threads))
print()
print(f"== Schedule comparison, compute time (s) ==")
print(f"{'schedule':<16}" + "".join(f"{p:>9d}" for p in threads))
for key in sorted(sched_keys):
    print(f"{key[1]:<16}" + "".join(f"{med(key, p, 1):9.3f}" for p in threads))
print(f"\nBest schedule at {pmax} threads: {best[1]}\n")

# ---- plots
def plot(ylabel, fname, fn, ideal):
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
    for a, col, T0, ttl in ((ax[0], 1, T_seq_c, "Compute phase only"), (ax[1], 2, T_seq_t, "End-to-end (load + compute)")):
        for name, key in main:
            ys = [fn(T0 / med(key, p, col), p) for p in threads]
            a.plot(threads, ys, marker="o", label=name)
        a.plot(threads, [ideal(p) for p in threads], "k--", label="ideal")
        a.set_xticks(threads); a.set_xlabel("threads"); a.set_ylabel(ylabel); a.set_title(ttl); a.grid(alpha=.3)
    ax[0].legend(fontsize=8)
    fig.tight_layout(); fig.savefig(fname, dpi=150); plt.close(fig)

plot("speedup S", "plots/speedup.png", lambda S, p: S, lambda p: p)
plot("efficiency E = S/p", "plots/efficiency.png", lambda S, p: S / p, lambda p: 1)

fig, ax = plt.subplots(figsize=(6.5, 4.2))
ks = sorted(sched_keys); bw = 0.8 / len(ks)
for i, key in enumerate(ks):
    ax.bar([j + i * bw for j in range(len(threads))], [med(key, p, 1) for p in threads], bw, label=key[1])
ax.set_xticks([j + 0.4 - bw / 2 for j in range(len(threads))]); ax.set_xticklabels(threads)
ax.set_xlabel("threads"); ax.set_ylabel("compute time (s)"); ax.set_title("OpenMP schedule comparison"); ax.legend(); ax.grid(alpha=.3, axis="y")
fig.tight_layout(); fig.savefig("plots/schedules.png", dpi=150); plt.close(fig)
print("wrote results/summary.csv, plots/speedup.png, plots/efficiency.png, plots/schedules.png")