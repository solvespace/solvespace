//-----------------------------------------------------------------------------
// SolveSpace threading utilities implementation
//
// Copyright 2008-2025 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "threaded.h"

namespace SolveSpace {

// The global thread pool instance
static std::unique_ptr<ThreadPool> globalThreadPool;
static std::once_flag threadingInitFlag;

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for(size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(
            [this] {
                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock,
                            [this] { return this->stop || !this->tasks.empty(); });
                        
                        if(this->stop && this->tasks.empty()) {
                            return;
                        }
                        
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    
                    task();
                }
            }
        );
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    
    for(std::thread &worker : workers) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}

size_t ThreadPool::Size() const {
    return workers.size();
}

void ThreadPool::WaitAll() {
    std::unique_lock<std::mutex> lock(queueMutex);
    condition.wait(lock, [this] { return tasks.empty(); });
}

ThreadPool& GetThreadPool() {
    std::call_once(threadingInitFlag, InitThreading);
    ssassert(globalThreadPool != nullptr, "Thread pool not initialized");
    return *globalThreadPool;
}

void InitThreading() {
    size_t numThreads = GetOptimalThreadCount();
    
    // Check user settings
    if(SS.enableMultiThreaded) {
        if(SS.threadCount > 0) {
            // Use user-specified thread count
            numThreads = SS.threadCount;
        }
    } else {
        // Threading disabled by user
        numThreads = 1;
    }
    
    globalThreadPool = std::make_unique<ThreadPool>(numThreads);
}

void ShutdownThreading() {
    globalThreadPool.reset();
}

size_t GetOptimalThreadCount() {
    unsigned int threadCount = std::thread::hardware_concurrency();
    if(threadCount == 0) {
        // If we can't detect, default to 2 threads
        return 2;
    }
    
    // Use at least 2 threads, but no more than the available cores
    return std::max(2u, threadCount);
}

} // namespace SolveSpace