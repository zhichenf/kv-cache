#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
        : running_(true) {
        if (thread_count == 0) thread_count = 4;
        
        for (size_t i = 0; i < thread_count; i++) {
            workers_.emplace_back([this] {
                while (running_) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { 
                            return !tasks_.empty() || !running_; 
                        });
                        
                        if (!running_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        running_ = false;
        cv_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    // 提交任务到线程池
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {      // submit后返回值可以在必要时候获取
        
        // 自动推导返回类型
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::lock_guard lock(mutex_);
            if (!running_) {
                throw std::runtime_error("Submit on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        cv_.notify_one();
        return result;
    }
    
    // 获取线程数
    size_t Size() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
};
