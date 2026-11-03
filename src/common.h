// Shared code for all parallel versions: parsing, file loading, output.
// Copied from seq.cpp so results are byte-identical. Only the compute loop differs per version.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
using namespace std;
using Clock = chrono::steady_clock;

static const int MAXC = 32, MAXA = 512;
static vector<string> carriers, airports;
static int8_t  carrier_lut[65536];
static int16_t airport_lut[32768];

// Flat counters (index = carrier*12 + month) so OpenMP array-section reductions work.
static long long c_flights[MAXC * 12], c_cancel[MAXC * 12], c_dep[MAXC * 12], c_arr[MAXC * 12];
static long long a_flown[MAXA], a_dep[MAXA];

static inline int carrier_key(const char* p) { return ((unsigned char)p[0] << 8) | (unsigned char)p[1]; }
static inline int airport_key(const char* p) { return ((p[0] & 31) << 10) | ((p[1] & 31) << 5) | (p[2] & 31); }

struct Rec { int month, carrier, origin, cancelled, dep, arr; };

static inline bool parse_line(const char* p, Rec& r) {
    const char* f[12]; int n = 0; f[n++] = p;
    while (n < 12) {
        while (*p != ',' && *p != '\n') ++p;
        if (*p != ',') break;
        ++p; f[n++] = p;
    }
    if (n < 12) return false;
    r.month     = atoi(f[1]);
    r.carrier   = carrier_lut[carrier_key(f[4])];
    r.origin    = airport_lut[airport_key(f[6])];
    r.cancelled = atoi(f[9]);
    r.dep       = atoi(f[10]);
    r.arr       = atoi(f[11]);
    return r.carrier >= 0 && r.origin >= 0 && r.month >= 1 && r.month <= 12;
}

static void load_list(const char* path, vector<string>& out) {
    ifstream f(path); string s;
    while (getline(f, s)) if (!s.empty()) out.push_back(s);
}

// compute(buf, off, n, nthreads) must fill the global counters and return the number of bad lines.
typedef long long (*ComputeFn)(const char* buf, const vector<size_t>& off, size_t n, int nthreads);

// usage: prog data.csv carriers.txt origins.txt out.csv [threads]
// threads defaults to $OMP_NUM_THREADS, else 1.
static int driver(int argc, char** argv, const char* name, ComputeFn compute) {
    if (argc < 5) { fprintf(stderr, "usage: %s data.csv carriers.txt origins.txt out.csv [threads]\n", argv[0]); return 1; }
    int nthreads = 1;
    if (const char* e = getenv("OMP_NUM_THREADS")) nthreads = max(1, atoi(e));
    if (argc >= 6) nthreads = max(1, atoi(argv[5]));

    load_list(argv[2], carriers);
    load_list(argv[3], airports);
    if ((int)carriers.size() > MAXC || (int)airports.size() > MAXA) { fprintf(stderr, "too many carriers/airports\n"); return 1; }
    memset(carrier_lut, -1, sizeof carrier_lut);
    memset(airport_lut, -1, sizeof airport_lut);
    for (size_t i = 0; i < carriers.size(); i++) carrier_lut[carrier_key(carriers[i].c_str())] = i;
    for (size_t i = 0; i < airports.size(); i++) airport_lut[airport_key(airports[i].c_str())] = i;

    // load file (serial, identical to seq.cpp)
    auto t0 = Clock::now();
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); size_t size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 2);
    if (fread(buf, 1, size, f) != size) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);
    if (size == 0 || buf[size - 1] != '\n') buf[size++] = '\n';
    buf[size] = 0;
    vector<size_t> off; off.reserve(size / 30);
    size_t start = 0;
    for (size_t i = 0; i < size; i++) if (buf[i] == '\n') { off.push_back(start); start = i + 1; }
    size_t n = off.size();
    auto t1 = Clock::now();

    // compute (the only part that differs between versions)
    long long bad = compute(buf, off, n, nthreads);
    auto t2 = Clock::now();

    // output (identical to seq.cpp)
    FILE* out = fopen(argv[4], "w");
    fprintf(out, "carrier,month,flights,cancelled,sum_dep,sum_arr\n");
    long long tot_flights = 0, tot_flown = 0, tot_dep = 0, tot_arr = 0;
    printf("\nCARRIER   flights  cancelled  avgDep  avgArr (minutes, non-cancelled)\n");
    for (size_t c = 0; c < carriers.size(); c++) {
        long long fl = 0, ca = 0, d = 0, a = 0;
        for (int m = 0; m < 12; m++) {
            int k = c * 12 + m;
            fprintf(out, "%s,%d,%lld,%lld,%lld,%lld\n", carriers[c].c_str(), m + 1,
                    c_flights[k], c_cancel[k], c_dep[k], c_arr[k]);
            fl += c_flights[k]; ca += c_cancel[k]; d += c_dep[k]; a += c_arr[k];
        }
        long long flown = fl - ca;
        printf("%-8s %9lld %9lld %7.2f %7.2f\n", carriers[c].c_str(), fl, ca,
               flown ? (double)d / flown : 0.0, flown ? (double)a / flown : 0.0);
        tot_flights += fl; tot_flown += flown; tot_dep += d; tot_arr += a;
    }
    fclose(out);

    vector<pair<double, int>> worst;
    for (size_t a = 0; a < airports.size(); a++)
        if (a_flown[a] >= 5000) worst.push_back({(double)a_dep[a] / a_flown[a], (int)a});
    sort(worst.rbegin(), worst.rend());
    printf("\nTOP 10 WORST ORIGIN AIRPORTS (avg departure delay, >=5000 flights)\n");
    for (size_t i = 0; i < worst.size() && i < 10; i++)
        printf("%2zu. %s  %.2f min\n", i + 1, airports[worst[i].second].c_str(), worst[i].first);

    printf("\nCHECK lines=%zu used=%lld bad=%lld flown=%lld sum_dep=%lld sum_arr=%lld\n",
           n, tot_flights, bad, tot_flown, tot_dep, tot_arr);
    auto s = [](Clock::time_point a, Clock::time_point b) { return chrono::duration<double>(b - a).count(); };
    printf("VARIANT %s THREADS %d\n", name, nthreads);
    printf("TIME_LOAD %.3f\nTIME_COMPUTE %.3f\nTIME_TOTAL %.3f\n", s(t0, t1), s(t1, t2), s(t0, t2));
    free(buf);
    return 0;
}