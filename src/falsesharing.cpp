#include <omp.h>
#include <cstdio>
#include <cstring>

struct alignas(64) Padded { volatile long long v; };
alignas(64) static volatile long long shared_c[8];
static Padded padded_c[8];

int main(int argc, char** argv) {
    const long long N = 100000000LL;
    omp_set_num_threads(8);
    bool run_shared = (argc < 2 || !strcmp(argv[1], "shared"));
    bool run_padded = (argc < 2 || !strcmp(argv[1], "padded"));

    if (run_shared) {
        double t0 = omp_get_wtime();
        #pragma omp parallel
        {
            int id = omp_get_thread_num();
            for (long long i = 0; i < N; i++) shared_c[id] = shared_c[id] + 1;
        }
        printf("shared,%.3f\n", omp_get_wtime() - t0);
    }
    if (run_padded) {
        double t0 = omp_get_wtime();
        #pragma omp parallel
        {
            int id = omp_get_thread_num();
            for (long long i = 0; i < N; i++) padded_c[id].v = padded_c[id].v + 1;
        }
        printf("padded,%.3f\n", omp_get_wtime() - t0);
    }
}