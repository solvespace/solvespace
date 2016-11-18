//-----------------------------------------------------------------------------
// Our harness for running benchmarks.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
#if defined(WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace SolveSpace {
    // These are defined in headless.cpp, and aren't exposed in solvespace.h.
    extern std::string resourceDir;
}

static std::string ResourceRoot() {
    static std::string rootDir;
    if(!rootDir.empty()) return rootDir;

    // No especially good way to do this, so let's assume somewhere up from
    // the current directory there's our repository, with CMakeLists.txt, and
    // pivot from there.
#if defined(WIN32)
    wchar_t currentDirW[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDirW);
    rootDir = Narrow(currentDirW);
#else
    rootDir = ".";
#endif

    // We're never more than four levels deep.
    for(size_t i = 0; i < 4; i++) {
        std::string listsPath = rootDir;
        listsPath += PATH_SEP;
        listsPath += "CMakeLists.txt";
        FILE *f = ssfopen(listsPath, "r");
        if(f) {
            fclose(f);
            rootDir += PATH_SEP;
            rootDir += "res";
            return rootDir;
        }

        if(rootDir[0] == '.') {
            rootDir += PATH_SEP;
            rootDir += "..";
        } else {
            rootDir.erase(rootDir.rfind(PATH_SEP));
        }
    }

    ssassert(false, "Couldn't locate repository root");
}

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
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#if defined(WIN32)
    InitHeaps();
#endif

    resourceDir = ResourceRoot();

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
                if(!SS.ReloadAllImported(/*canCancel=*/false))
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
