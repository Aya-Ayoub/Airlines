// OpenMP version 2 (comparison): ONE shared set of counters, every update protected by
// `#pragma omp atomic`. Correct, but all threads fight over the same cache lines, so it is
// expected to scale badly. Static schedule so the only difference vs omp_reduction is the sync.
#include <omp.h>
#include "common.h"

static long long compute(const char* buf, const vector<size_t>& off, size_t n, int nthreads) {
    omp_set_num_threads(nthreads);
    long long bad = 0;

    #pragma omp parallel for schedule(static) reduction(+ : bad)
    for (size_t i = 0; i < n; i++) {
        Rec r;
        if (!parse_line(buf + off[i], r)) { bad++; continue; }
        int k = r.carrier * 12 + (r.month - 1);
        #pragma omp atomic
        c_flights[k]++;
        if (r.cancelled) {
            #pragma omp atomic
            c_cancel[k]++;
        } else {
            #pragma omp atomic
            c_dep[k] += r.dep;
            #pragma omp atomic
            c_arr[k] += r.arr;
            #pragma omp atomic
            a_flown[r.origin]++;
            #pragma omp atomic
            a_dep[r.origin] += r.dep;
        }
    }
    return bad;
}

int main(int argc, char** argv) { return driver(argc, argv, "omp_atomic", compute); }