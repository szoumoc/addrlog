#include <iostream>
#include <random>
#include <algorithm>
// #include "memtrace_rt.h"

extern "C" void analyzeAndPrint(); // 

int main() {
    const int N = 10000;

    int* data    = new int[N];
    int* data2   = new int[N];
    int* indices = new int[N];

    for (int i = 0; i < N; ++i) {
        data[i]    = i;
        data2[i]   = i;
        indices[i] = i;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices, indices + N, gen);

    // sequential
    for (int i = 0; i < N; ++i) {
        int x = data[i];
        data[i] = x + 1;
    }

    // random
    for (int i = 0; i < N; ++i) {
        int idx   = indices[i];
        int x     = data2[idx];
        data2[idx] = x + 1;
    }

    analyzeAndPrint();

    delete[] data;
    delete[] data2;
    delete[] indices;
    return 0;
}