// Version 4: plain C++ std::thread + std::atomic, no OpenMP.
//  - Work distribution: a shared std::atomic<size_t> hands out chunks of lines (dynamic scheduling).
//  - Each thread counts into its OWN private (cache-line aligned) counters.
//  - At the end each thread merges its counters into the global arrays with atomic fetch_add
//    (std::atomic_ref, C++20). Only ~2.7k atomic adds per thread, so merge cost is negligible.
// Build with -std=c++20 -pthread.
#include <atomic>
#include <thread>
#include "common.h"

struct alignas(64) Local {
    long long cf[MAXC * 12], cc[MAXC * 12], cd[MAXC * 12], ca[MAXC * 12];
    long long af[MAXA], ad[MAXA];
    long long bad;
};

static const size_t CHUNK = 4096;   // lines per grab; large enough that the atomic counter is not a hotspot

static inline void add(long long& g, long long v) {
    if (v) std::atomic_ref<long long>(g).fetch_add(v, std::memory_order_relaxed);
}

static long long compute(const char* buf, const vector<size_t>& off, size_t n, int nthreads) {
    std::atomic<size_t> next{0};
    std::atomic<long long> bad_total{0};
    vector<Local> locals(nthreads);       // zero-initialised by value-initialisation
    memset((void*)locals.data(), 0, sizeof(Local) * nthreads);

    auto worker = [&](int t) {
        Local& L = locals[t];
        for (;;) {
            size_t lo = next.fetch_add(CHUNK, std::memory_order_relaxed);
            if (lo >= n) break;
            size_t hi = min(lo + CHUNK, n);
            for (size_t i = lo; i < hi; i++) {
                Rec r;
                if (!parse_line(buf + off[i], r)) { L.bad++; continue; }
                int k = r.carrier * 12 + (r.month - 1);
                L.cf[k]++;
                if (r.cancelled) {
                    L.cc[k]++;
                } else {
                    L.cd[k] += r.dep;
                    L.ca[k] += r.arr;
                    L.af[r.origin]++;
                    L.ad[r.origin] += r.dep;
                }
            }
        }
        // merge private counters into the global ones atomically
        for (int k = 0; k < MAXC * 12; k++) {
            add(c_flights[k], L.cf[k]); add(c_cancel[k], L.cc[k]);
            add(c_dep[k], L.cd[k]);     add(c_arr[k], L.ca[k]);
        }
        for (int a = 0; a < MAXA; a++) { add(a_flown[a], L.af[a]); add(a_dep[a], L.ad[a]); }
        bad_total.fetch_add(L.bad, std::memory_order_relaxed);
    };

    vector<std::thread> pool;
    for (int t = 1; t < nthreads; t++) pool.emplace_back(worker, t);
    worker(0);                              // main thread works too
    for (auto& th : pool) th.join();
    return bad_total.load();
}

int main(int argc, char** argv) { return driver(argc, argv, "std_thread", compute); }