// OpenMP version 1 (the main one): every thread gets PRIVATE copies of all counters,
// OpenMP merges them with a reduction(+) after the loop. No synchronisation inside the loop.
//
// Schedule is chosen at run time so we can compare static / dynamic / guided without recompiling:
//   OMP_SCHEDULE="static" ./build/omp_reduction ...
//   OMP_SCHEDULE="dynamic,4096" ./build/omp_reduction ...
//   OMP_SCHEDULE="guided" ./build/omp_reduction ...
#include <omp.h>
#include "common.h"

static long long compute(const char* buf, const vector<size_t>& off, size_t n, int nthreads) {
    omp_set_num_threads(nthreads);
    long long bad = 0;
    // array-section reductions need pointers/arrays; these point at the flat global arrays
    long long *cf = c_flights, *cc = c_cancel, *cd = c_dep, *ca = c_arr, *af = a_flown, *ad = a_dep;

    #pragma omp parallel for schedule(runtime) \
        reduction(+ : bad, cf[:MAXC * 12], cc[:MAXC * 12], cd[:MAXC * 12], ca[:MAXC * 12], af[:MAXA], ad[:MAXA])
    for (size_t i = 0; i < n; i++) {
        Rec r;
        if (!parse_line(buf + off[i], r)) { bad++; continue; }
        int k = r.carrier * 12 + (r.month - 1);
        cf[k]++;
        if (r.cancelled) {
            cc[k]++;
        } else {
            cd[k] += r.dep;
            ca[k] += r.arr;
            af[r.origin]++;
            ad[r.origin] += r.dep;
        }
    }
    return bad;
}

int main(int argc, char** argv) { return driver(argc, argv, "omp_reduction", compute); }