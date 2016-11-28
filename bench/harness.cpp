//-----------------------------------------------------------------------------
// Our harness for running benchmarks.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

static bool RunBenchmark(std::function<void()> setupFn,
                         std::function<bool()> benchFn,
                         std::function<void()> teardownFn,
                         size_t minIter = 5, double minTime = 5.0) {
    // Warmup
    setupFn();
    if(!benchFn()) {
        fprintf(stderr, "Benchmark failed\n");
        return false;
    }
    teardownFn();

    // Benchmark
    size_t iter = 0;
    double time = 0.0;
    while(iter < minIter || time < minTime) {
        setupFn();
        auto testStartTime = std::chrono::steady_clock::now();
        benchFn();
        auto testEndTime = std::chrono::steady_clock::now();
        teardownFn();

        std::chrono::duration<double> testTime = testEndTime - testStartTime;
        time += testTime.count();
        iter += 1;
    }

    // Report
    fprintf(stdout, "Iterations: %zd\n", iter);
    fprintf(stdout, "Time:       %.3f s\n", time);
    fprintf(stdout, "Per iter.:  %.3f s\n", time / (double)iter);

    return true;
}

int main(int argc, char **argv) {
    InitPlatform();

    std::string mode, filename;
    if(argc == 3) {
        mode = argv[1];
        filename = argv[2];
    } else {
        fprintf(stderr, "Usage: %s [mode] [filename]\n", argv[0]);
        fprintf(stderr, "Mode can be one of: load.\n");
        return 1;
    }

    bool result = false;
    if(mode == "load") {
        result = RunBenchmark(
            [] {
                SS.Init();
            },
            [&] {
                if(!SS.LoadFromFile(filename))
                    return false;
                SS.AfterNewFile();
                return true;
            },
            [] {
                SK.Clear();
                SS.Clear();
            });
    } else {
        fprintf(stderr, "Unknown mode \"%s\"\n", mode.c_str());
    }

    return (result == true ? 0 : 1);
}
