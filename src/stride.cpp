#include <chrono>
#include <cstdio>
#include <vector>
int main() {
    const size_t n = 64u * 1024 * 1024;     
    std::vector<int> a(n, 1);
    long long sink = 0;
    printf("stride,time_s\n");
    for (size_t stride = 1; stride <= 1024; stride *= 2) {
        auto t0 = std::chrono::steady_clock::now();
        long long sum = 0;
        for (size_t s = 0; s < stride; s++)
            for (size_t i = s; i < n; i += stride) sum += a[i];
        auto t1 = std::chrono::steady_clock::now();
        sink += sum;
        printf("%zu,%.3f\n", stride, std::chrono::duration<double>(t1 - t0).count());
    }
    fprintf(stderr, "check %lld\n", sink);
}