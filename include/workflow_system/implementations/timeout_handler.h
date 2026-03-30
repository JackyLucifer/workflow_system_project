/**
 * @file timeout_handler.h
 * @brief 业务流程管理系统 - 超时处理器实现
 *
 * 设计模式：
 * - 策略模式：实现不同的超时处理策略
 *
 * 面向对象：
 * - 封装：封装超时计时和处理逻辑
 */

#ifndef WORKFLOW_SYSTEM_TIMEOUT_HANDLER_IMPL_H
#define WORKFLOW_SYSTEM_TIMEOUT_HANDLER_IMPL_H

#include "workflow_system/interfaces/timeout_handler.h"
#include "workflow_system/core/logger.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>

namespace WorkflowSystem {

/**
 * @brief 工作流接口前向声明
 */
class IWorkflow;

/**
 * @brief 超时处理器实现
 */
class TimeoutHandler : public ITimeoutHandler {
public:
    explicit TimeoutHandler(const std::string& workflowName,
                          std::weak_ptr<IWorkflow> workflow)
        : workflowName_(workflowName), workflow_(workflow),
          timeoutMs_(0), policy_(TimeoutPolicy::INTERRUPT),
          timedOut_(false), timerRunning_(false),
          startTime_(0), elapsedMs_(0),
          retryCount_(0) {}

    virtual ~TimeoutHandler() {
        stopTimer();
    }

    void setTimeout(int64_t timeoutMs) override {
        std::lock_guard<std::mutex> lock(mutex_);
        timeoutMs_ = timeoutMs;
        LOG_INFO("[" + workflowName_ + "] Timeout set to " +
                  std::to_string(timeoutMs_) + "ms");
    }

    int64_t getTimeout() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return timeoutMs_;
    }

    void setPolicy(TimeoutPolicy policy) override {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_ = policy;
    }

    TimeoutPolicy getPolicy() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return policy_;
    }

    void setTimeoutCallback(TimeoutCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = callback;
    }

    void startTimer() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (timeoutMs_ <= 0) {
            LOG_INFO("[" + workflowName_ + "] No timeout set, timer not started");
            return;
        }

        if (timerRunning_) {
            LOG_WARNING("[" + workflowName_ + "] Timer already running");
            return;
        }

        timedOut_ = false;
        timerRunning_ = true;
        startTime_ = getCurrentTimeMs();
        elapsedMs_ = 0;

        LOG_INFO("[" + workflowName_ + "] Timeout timer started (" +
                  std::to_string(timeoutMs_) + "ms)");

        // 启动监控线程
        timerThread_ = std::thread(&TimeoutHandler::monitorTimeout, this);
    }

    void stopTimer() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!timerRunning_) {
                return;
            }
            timerRunning_ = false;
        }

        // 通知监控线程退出
        cv_.notify_all();

        // 等待监控线程结束
        if (timerThread_.joinable()) {
            timerThread_.join();
        }

        LOG_INFO("[" + workflowName_ + "] Timeout timer stopped");
    }

    void resetTimer() override {
        std::lock_guard<std::mutex> lock(mutex_);
        startTime_ = getCurrentTimeMs();
        elapsedMs_ = 0;
        timedOut_ = false;
        LOG_INFO("[" + workflowName_ + "] Timer reset");
    }

    bool isTimedOut() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return timedOut_;
    }

    int64_t getElapsedTime() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (timerRunning_) {
            return getCurrentTimeMs() - startTime_;
        }
        return elapsedMs_;
    }

    int64_t getRemainingTime() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (timeoutMs_ <= 0) {
            return -1;  // 无超时限制
        }
        if (timerRunning_) {
            int64_t elapsed = getCurrentTimeMs() - startTime_;
            return std::max(int64_t(0), timeoutMs_ - elapsed);
        }
        return std::max(int64_t(0), timeoutMs_ - elapsedMs_);
    }

    /**
     * @brief 设置超时配置
     */
    void setConfig(const TimeoutConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        timeoutMs_ = config.timeoutMs;
        policy_ = config.policy;
        callback_ = config.callback;
        autoRestart_ = config.autoRestart;
        maxRetries_ = config.maxRetries;
        retryCount_ = 0;
    }

    /**
     * @brief 获取重试次数
     */
    int getRetryCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return retryCount_;
    }

    /**
     * @brief 重置重试计数
     */
    void resetRetryCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        retryCount_ = 0;
    }

    /**
     * @brief 设置最大重试次数
     */
    void setMaxRetries(int maxRetries) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxRetries_ = maxRetries;
    }

private:
    /**
     * @brief 监控超时的线程函数
     */
    void monitorTimeout() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);

            if (!timerRunning_) {
                break;
            }

            // 计算剩余时间
            int64_t elapsed = getCurrentTimeMs() - startTime_;
            elapsedMs_ = elapsed;

            if (elapsed >= timeoutMs_) {
                // 超时了
                timedOut_ = true;
                timerRunning_ = false;
                elapsedMs_ = timeoutMs_;

                LOG_WARNING("[" + workflowName_ + "] Workflow timed out after " +
                           std::to_string(timeoutMs_) + "ms");

                lock.unlock();

                // 执行超时回调
                if (callback_) {
                    callback_(workflowName_, timeoutMs_);
                }

                // 根据策略执行操作
                handleTimeout();
                break;
            }

            // 等待一段时间再检查
            int64_t remaining = timeoutMs_ - elapsed;
            int64_t waitTime = std::min(int64_t(100), remaining);  // 每100ms检查一次

            cv_.wait_for(lock, std::chrono::milliseconds(waitTime),
                        [this]() { return !timerRunning_; });
        }
    }

    /**
     * @brief 处理超时
     */
    void handleTimeout() {
        auto workflow = workflow_.lock();
        if (!workflow) {
            LOG_ERROR("[" + workflowName_ + "] Workflow expired, cannot handle timeout");
            return;
        }

        switch (policy_) {
            case TimeoutPolicy::INTERRUPT:
                LOG_INFO("[" + workflowName_ + "] Interrupting workflow due to timeout");
                workflow->interrupt();
                break;

            case TimeoutPolicy::PAUSE:
                LOG_INFO("[" + workflowName_ + "] Pausing workflow due to timeout");
                // workflow->pause();  // 需要在 IWorkflow 接口中添加 pause 方法
                break;

            case TimeoutPolicy::RETRY:
                if (autoRestart_ && retryCount_ < maxRetries_) {
                    retryCount_++;
                    LOG_INFO("[" + workflowName_ + "] Retrying workflow (attempt " +
                              std::to_string(retryCount_) + "/" + std::to_string(maxRetries_) + ")");
                    // workflow->interrupt();
                    // workflow->start(context_);  // 需要保存上下文
                } else {
                    LOG_WARNING("[" + workflowName_ + "] Max retries exceeded, interrupting workflow");
                    workflow->interrupt();
                }
                break;

            case TimeoutPolicy::NOTIFY_ONLY:
                LOG_INFO("[" + workflowName_ + "] Timeout notification sent, workflow continues");
                break;

            default:
                LOG_WARNING("[" + workflowName_ + "] Unknown timeout policy");
                break;
        }
    }

    /**
     * @brief 获取当前时间（毫秒）
     */
    static int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

private:
    std::string workflowName_;
    std::weak_ptr<IWorkflow> workflow_;

    int64_t timeoutMs_;
    TimeoutPolicy policy_;
    TimeoutCallback callback_;
    bool autoRestart_;
    int maxRetries_;

    std::atomic<bool> timedOut_;
    std::atomic<bool> timerRunning_;
    int64_t startTime_;
    int64_t elapsedMs_;
    int retryCount_;

    std::thread timerThread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * @brief 超时处理器工厂
 */
class TimeoutHandlerFactory {
public:
    static std::shared_ptr<TimeoutHandler> create(
        const std::string& workflowName,
        std::weak_ptr<IWorkflow> workflow) {
        return std::make_shared<TimeoutHandler>(workflowName, workflow);
    }

    static std::shared_ptr<TimeoutHandler> createWithConfig(
        const std::string& workflowName,
        std::weak_ptr<IWorkflow> workflow,
        const TimeoutConfig& config) {
        auto handler = std::make_shared<TimeoutHandler>(workflowName, workflow);
        handler->setConfig(config);
        return handler;
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_TIMEOUT_HANDLER_IMPL_H
