#ifndef PLUGIN_FRAMEWORK_THREAD_POOL_HPP
#define PLUGIN_FRAMEWORK_THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <map>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 线程池
 * 
 * 管理工作线程，执行异步任务
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param threadCount 工作线程数量（0表示自动检测）
     */
    explicit ThreadPool(size_t threadCount = 0);
    
    /**
     * @brief 析构函数
     */
    ~ThreadPool();
    
    /**
     * @brief 提交任务
     * @param task 任务函数
     * @return future对象，用于获取结果
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    /**
     * @brief 提交无返回值的任务
     */
    void execute(std::function<void()> task);
    
    /**
     * @brief 等待所有任务完成
     */
    void waitAll();
    
    /**
     * @brief 获取工作线程数量
     */
    size_t getThreadCount() const { return workers_.size(); }
    
    /**
     * @brief 获取待处理任务数量
     */
    size_t getPendingTaskCount() const;
    
    /**
     * @brief 获取活跃线程数量
     */
    size_t getActiveThreadCount() const;
    
    /**
     * @brief 启动线程池
     */
    void start();
    
    /**
     * @brief 停止线程池
     */
    void stop();
    
    /**
     * @brief 检查线程池是否运行中
     */
    bool isRunning() const { return running_.load(); }

private:
    void workerThread();
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable finishedCondition_;
    
    std::atomic<bool> running_;
    std::atomic<size_t> activeThreads_;
    size_t threadCount_;
};

// 模板实现
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using ReturnType = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<ReturnType> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        
        if (!running_.load()) {
            throw std::runtime_error("线程池未运行");
        }
        
        tasks_.emplace([task]() { (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

/**
 * @brief 定时器ID
 */
using TimerId = uint64_t;

/**
 * @brief 定时器服务
 * 
 * 提供延迟执行和周期执行功能
 */
class TimerService {
public:
    TimerService();
    ~TimerService();
    
    /**
     * @brief 延迟执行任务
     * @param delayMs 延迟时间（毫秒）
     * @param callback 回调函数
     * @return 定时器ID
     */
    TimerId scheduleDelayed(uint64_t delayMs, std::function<void()> callback);
    
    /**
     * @brief 周期执行任务
     * @param intervalMs 执行间隔（毫秒）
     * @param callback 回调函数
     * @return 定时器ID
     */
    TimerId scheduleInterval(uint64_t intervalMs, std::function<void()> callback);
    
    /**
     * @brief 取消定时器
     * @param timerId 定时器ID
     * @return 成功返回true
     */
    bool cancel(TimerId timerId);
    
    /**
     * @brief 启动定时器服务
     */
    void start();
    
    /**
     * @brief 停止定时器服务
     */
    void stop();
    
    /**
     * @brief 检查定时器服务是否运行中
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief 获取活跃定时器数量
     */
    size_t getActiveTimerCount() const;

private:
    void workerThread();
    
    struct TimerEntry {
        TimerId id;
        std::function<void()> callback;
        std::chrono::system_clock::time_point nextRun;
        uint64_t interval;
        bool periodic;
        bool cancelled;
    };
    
    std::thread worker_;
    std::map<TimerId, std::shared_ptr<TimerEntry>> timers_;
    mutable std::mutex timersMutex_;
    std::condition_variable condition_;
    
    std::atomic<bool> running_;
    std::atomic<TimerId> nextId_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_THREAD_POOL_HPP
