//-----------------------------------------------------------------------------
// SolveSpace threading utilities
//
// Copyright 2008-2025 Jonathan Westhues.
//-----------------------------------------------------------------------------
#ifndef __THREADED_H
#define __THREADED_H

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <future>

namespace SolveSpace {

/**
 * A simple thread pool implementation for SolveSpace.
 */
class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    /**
     * Submit a task to be executed by the thread pool.
     * Returns a future that will contain the result of the task.
     */
    template<class F, class... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if(stop) {
                throw std::runtime_error("Cannot add task to stopped ThreadPool");
            }
            
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return result;
    }

    /**
     * Submit a batch of tasks to be executed by the thread pool.
     * Returns a vector of futures for the results.
     */
    template<class F, class Iterator>
    auto SubmitBatch(F&& f, Iterator begin, Iterator end) -> std::vector<std::future<decltype(f(*begin))>> {
        using ReturnType = decltype(f(*begin));
        std::vector<std::future<ReturnType>> results;
        results.reserve(std::distance(begin, end));

        for(auto it = begin; it != end; ++it) {
            results.push_back(Submit(f, *it));
        }

        return results;
    }

    /**
     * Returns the number of threads in the pool.
     */
    size_t Size() const;

    /**
     * Wait for all tasks to complete.
     */
    void WaitAll();

private:
    bool stop;
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queueMutex;
    std::condition_variable condition;
};

/**
 * Get the default thread pool instance.
 */
ThreadPool& GetThreadPool();

/**
 * Initialize the threading subsystem.
 */
void InitThreading();

/**
 * Shutdown the threading subsystem.
 */
void ShutdownThreading();

/**
 * Get the optimal number of threads for the current system.
 */
size_t GetOptimalThreadCount();

} // namespace SolveSpace

#endif // __THREADED_H