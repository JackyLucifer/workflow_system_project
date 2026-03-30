#include "workflow_system/plugin/utils/ThreadPool.hpp"
#include "workflow_system/core/logger.h"
#include <algorithm>

namespace WorkflowSystem { namespace Plugin {

// ==================== ThreadPool 实现 ====================

ThreadPool::ThreadPool(size_t threadCount)
    : running_(false)
    , activeThreads_(0) {
    
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) {
            threadCount = 4;  // 默认4个线程
        }
    }
    
    threadCount_ = threadCount;
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    
    for (size_t i = 0; i < threadCount_; ++i) {
        workers_.emplace_back([this]() {
            workerThread();
        });
    }
    
    LOG_INFO("线程池已启动，线程数: " + std::to_string(threadCount_));
}

void ThreadPool::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
    
    // 清空任务队列
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!tasks_.empty()) {
        tasks_.pop();
    }
    
    LOG_INFO("线程池已停止");
}

void ThreadPool::execute(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        
        if (!running_.load()) {
            LOG_WARNING("线程池未运行，无法执行任务");
            return;
        }
        
        tasks_.push(task);
    }
    
    condition_.notify_one();
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    finishedCondition_.wait(lock, [this]() {
        return tasks_.empty() && activeThreads_.load() == 0;
    });
}

size_t ThreadPool::getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

size_t ThreadPool::getActiveThreadCount() const {
    return activeThreads_.load();
}

void ThreadPool::workerThread() {
    while (running_.load()) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this]() {
                return !tasks_.empty() || !running_.load();
            });
            
            if (!running_.load()) {
                break;
            }
            
            if (tasks_.empty()) {
                continue;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
            activeThreads_++;
        }
        
        // 执行任务
        try {
            task();
        } catch (const std::exception& e) {
            LOG_ERROR("线程池任务执行异常: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("线程池任务执行未知异常");
        }
        
        activeThreads_--;
        finishedCondition_.notify_all();
    }
}

// ==================== TimerService 实现 ====================

TimerService::TimerService()
    : running_(false)
    , nextId_(1) {
}

TimerService::~TimerService() {
    stop();
}

TimerId TimerService::scheduleDelayed(uint64_t delayMs, std::function<void()> callback) {
    auto entry = std::make_shared<TimerEntry>();
    entry->id = nextId_++;
    entry->callback = callback;
    entry->nextRun = std::chrono::system_clock::now() + std::chrono::milliseconds(delayMs);
    entry->interval = 0;
    entry->periodic = false;
    entry->cancelled = false;
    
    {
        std::lock_guard<std::mutex> lock(timersMutex_);
        timers_[entry->id] = entry;
    }
    
    condition_.notify_one();
    
    LOG_INFO("创建延迟定时器: " + std::to_string(entry->id) + 
             " 延迟: " + std::to_string(delayMs) + "ms");
    
    return entry->id;
}

TimerId TimerService::scheduleInterval(uint64_t intervalMs, std::function<void()> callback) {
    auto entry = std::make_shared<TimerEntry>();
    entry->id = nextId_++;
    entry->callback = callback;
    entry->nextRun = std::chrono::system_clock::now() + std::chrono::milliseconds(intervalMs);
    entry->interval = intervalMs;
    entry->periodic = true;
    entry->cancelled = false;
    
    {
        std::lock_guard<std::mutex> lock(timersMutex_);
        timers_[entry->id] = entry;
    }
    
    condition_.notify_one();
    
    LOG_INFO("创建周期定时器: " + std::to_string(entry->id) + 
             " 间隔: " + std::to_string(intervalMs) + "ms");
    
    return entry->id;
}

bool TimerService::cancel(TimerId timerId) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    
    auto it = timers_.find(timerId);
    if (it == timers_.end()) {
        return false;
    }
    
    it->second->cancelled = true;
    timers_.erase(it);
    
    LOG_INFO("取消定时器: " + std::to_string(timerId));
    
    return true;
}

void TimerService::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    worker_ = std::thread([this]() {
        workerThread();
    });
    
    LOG_INFO("定时器服务已启动");
}

void TimerService::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    condition_.notify_all();
    
    if (worker_.joinable()) {
        worker_.join();
    }
    
    // 清空所有定时器
    std::lock_guard<std::mutex> lock(timersMutex_);
    timers_.clear();
    
    LOG_INFO("定时器服务已停止");
}

size_t TimerService::getActiveTimerCount() const {
    std::lock_guard<std::mutex> lock(timersMutex_);
    return timers_.size();
}

void TimerService::workerThread() {
    while (running_.load()) {
        std::vector<std::shared_ptr<TimerEntry>> readyTimers;
        std::chrono::system_clock::time_point nextTime;
        
        {
            std::unique_lock<std::mutex> lock(timersMutex_);
            
            auto now = std::chrono::system_clock::now();
            nextTime = now + std::chrono::hours(24);  // 默认等待24小时
            
            // 查找准备好的定时器
            for (auto it = timers_.begin(); it != timers_.end();) {
                auto& entry = it->second;
                
                if (entry->cancelled) {
                    it = timers_.erase(it);
                    continue;
                }
                
                if (entry->nextRun <= now) {
                    readyTimers.push_back(entry);
                    
                    if (entry->periodic) {
                        entry->nextRun = now + std::chrono::milliseconds(entry->interval);
                        ++it;
                    } else {
                        it = timers_.erase(it);
                    }
                } else {
                    if (entry->nextRun < nextTime) {
                        nextTime = entry->nextRun;
                    }
                    ++it;
                }
            }
            
            // 如果没有准备好的定时器，等待到下一个定时器时间
            if (readyTimers.empty() && !timers_.empty() && running_.load()) {
                condition_.wait_until(lock, nextTime);
                continue;
            }
            
            // 如果没有定时器，等待通知
            if (timers_.empty() && running_.load()) {
                condition_.wait(lock);
            }
        }
        
        // 执行准备好的定时器回调
        for (auto& entry : readyTimers) {
            if (entry->cancelled) {
                continue;
            }
            
            try {
                entry->callback();
            } catch (const std::exception& e) {
                LOG_ERROR("定时器回调执行异常: " + std::string(e.what()));
            } catch (...) {
                LOG_ERROR("定时器回调执行未知异常");
            }
        }
    }
}

} // namespace Plugin
} // namespace WorkflowSystem
