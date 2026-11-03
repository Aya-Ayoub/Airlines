// OpenMP version 3 (comparison): shared counters, the whole update block is one
// `#pragma omp critical` section (a lock). Only one thread can update at a time, so this is
// expected to be the slowest parallel version (parsing stays parallel, updating is serial).
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
        #pragma omp critical(update_counters)
        {
            c_flights[k]++;
            if (r.cancelled) {
                c_cancel[k]++;
            } else {
                c_dep[k] += r.dep;
                c_arr[k] += r.arr;
                a_flown[r.origin]++;
                a_dep[r.origin] += r.dep;
            }
        }
    }
    return bad;
}

int main(int argc, char** argv) { return driver(argc, argv, "omp_critical", compute); }