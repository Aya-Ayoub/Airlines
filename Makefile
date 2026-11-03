# Compile flags: set OPT to whatever Person A used for seq (e.g. make OPT="-O3")
.RECIPEPREFIX = >
OPT ?= -O2
CXX ?= g++

all: build/seq build/omp_reduction build/omp_atomic build/omp_critical build/std_thread

build:
> mkdir -p build

build/seq: src/seq.cpp | build
> $(CXX) $(OPT) -o $@ $<

build/omp_%: src/omp_%.cpp src/common.h | build
> $(CXX) $(OPT) -fopenmp -o $@ $<

build/std_thread: src/std_thread.cpp src/common.h | build
> $(CXX) $(OPT) -std=c++20 -pthread -o $@ $<

clean:
> rm -rf build