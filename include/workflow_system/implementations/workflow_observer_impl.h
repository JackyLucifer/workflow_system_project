/**
 * @file workflow_observer_impl.h
 * @brief 业务流程管理系统 - 工作流观察者实现
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_OBSERVER_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_OBSERVER_IMPL_H

#include "workflow_system/interfaces/workflow_observer.h"
#include "workflow_system/core/logger.h"
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <map>
#include <chrono>
#include <iostream>

namespace WorkflowSystem {

/**
 * @brief 工作流观察者基类
 * 
 * 提供默认的空实现，子类可以选择性重写
 */
class BaseWorkflowObserver : public IWorkflowObserver {
public:
    ~BaseWorkflowObserver() override = default;

    void onWorkflowStarted(const std::string& workflowName) override {
        // 默认空实现，子类可选择性重写
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        // 默认空实现，子类可选择性重写
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        // 默认空实现，子类可选择性重写
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        // 默认空实现，子类可选择性重写
    }
};

/**
 * @brief 日志观察者
 * 
 * 将工作流事件记录到日志系统
 */
class LoggingObserver : public IWorkflowObserver {
public:
    explicit LoggingObserver(bool verbose = false) : verbose_(verbose) {}

    void onWorkflowStarted(const std::string& workflowName) override {
        LOG_INFO("[Observer] Workflow started: " + workflowName);
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        LOG_INFO("[Observer] Workflow finished: " + workflowName);
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        LOG_WARNING("[Observer] Workflow interrupted: " + workflowName);
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        LOG_ERROR("[Observer] Workflow '" + workflowName + "' error: " + error);
    }

private:
    bool verbose_;
};

/**
 * @brief 控制台观察者
 * 
 * 将工作流事件输出到控制台，带有颜色标识
 */
class ConsoleObserver : public IWorkflowObserver {
public:
    enum class ConsoleColor {
        GREEN,    // 成功
        RED,      // 错误
        YELLOW,   // 警告
        BLUE,     // 信息
        RESET     // 重置
    };

    ConsoleObserver(bool useColor = true) : useColor_(useColor) {}

    void onWorkflowStarted(const std::string& workflowName) override {
        printColored("🚀 Workflow Started: ", workflowName, ConsoleColor::BLUE);
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        printColored("✅ Workflow Finished: ", workflowName, ConsoleColor::GREEN);
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        printColored("⏸️  Workflow Interrupted: ", workflowName, ConsoleColor::YELLOW);
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        printColored("❌ Workflow Error [" + workflowName + "]: ", error, ConsoleColor::RED);
    }

private:
    void printColored(const std::string& prefix, const std::string& message, ConsoleColor color) {
        if (useColor_) {
            std::cout << getColorCode(color) << prefix << message << getColorCode(ConsoleColor::RESET) << std::endl;
        } else {
            std::cout << prefix << message << std::endl;
        }
    }

    std::string getColorCode(ConsoleColor color) {
        switch (color) {
            case ConsoleColor::GREEN:  return "\033[32m";
            case ConsoleColor::RED:    return "\033[31m";
            case ConsoleColor::YELLOW: return "\033[33m";
            case ConsoleColor::BLUE:   return "\033[34m";
            case ConsoleColor::RESET:  return "\033[0m";
        }
        return "";
    }

    bool useColor_;
};

/**
 * @brief 统计观察者
 * 
 * 收集工作流执行统计数据
 */
class StatisticsObserver : public IWorkflowObserver {
public:
    struct WorkflowStats {
        std::string name;
        int startCount;
        int finishCount;
        int interruptCount;
        int errorCount;
        int64_t totalDuration;

        WorkflowStats()
            : startCount(0), finishCount(0),
              interruptCount(0), errorCount(0),
              totalDuration(0) {}
    };

    StatisticsObserver() : stats_() {}

    void onWorkflowStarted(const std::string& workflowName) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stat = stats_[workflowName];
        stat.name = workflowName;
        stat.startCount++;
        startTime_[workflowName] = std::chrono::steady_clock::now();
        
        LOG_INFO("[Stats] Workflow started: " + workflowName + 
                 " (Total starts: " + std::to_string(stat.startCount) + ")");
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stat = stats_[workflowName];
        stat.finishCount++;

        // 计算持续时间
        auto it = startTime_.find(workflowName);
        if (it != startTime_.end()) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second).count();
            stat.totalDuration += duration;
            startTime_.erase(it);
            
            LOG_INFO("[Stats] Workflow finished: " + workflowName + 
                     " (Duration: " + std::to_string(duration) + "ms)");
        }
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stat = stats_[workflowName];
        stat.interruptCount++;
        
        LOG_INFO("[Stats] Workflow interrupted: " + workflowName + 
                 " (Interrupts: " + std::to_string(stat.interruptCount) + ")");
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stat = stats_[workflowName];
        stat.errorCount++;
        
        LOG_INFO("[Stats] Workflow error: " + workflowName + 
                 " (Errors: " + std::to_string(stat.errorCount) + ")");
    }

    /**
     * @brief 获取指定工作流的统计
     */
    WorkflowStats getStats(const std::string& workflowName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stats_.find(workflowName);
        return it != stats_.end() ? it->second : WorkflowStats();
    }

    /**
     * @brief 获取所有工作流的统计
     */
    std::vector<WorkflowStats> getAllStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<WorkflowStats> result;
        for (const auto& pair : stats_) {
            result.push_back(pair.second);
        }
        return result;
    }

    /**
     * @brief 打印统计报告
     */
    void printReport() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== 工作流统计报告 ===" << std::endl;
        
        for (const auto& pair : stats_) {
            const auto& stat = pair.second;
            std::cout << "\n工作流: " << stat.name << std::endl;
            std::cout << "  启动次数: " << stat.startCount << std::endl;
            std::cout << "  完成次数: " << stat.finishCount << std::endl;
            std::cout << "  中断次数: " << stat.interruptCount << std::endl;
            std::cout << "  错误次数: " << stat.errorCount << std::endl;
            std::cout << "  总耗时: " << stat.totalDuration << "ms" << std::endl;
            
            if (stat.finishCount > 0) {
                double avgDuration = static_cast<double>(stat.totalDuration) / stat.finishCount;
                std::cout << "  平均耗时: " << avgDuration << "ms" << std::endl;
            }
        }
        std::cout << "====================\n" << std::endl;
    }

    /**
     * @brief 清除统计
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.clear();
        startTime_.clear();
    }

private:
    std::map<std::string, WorkflowStats> stats_;
    std::map<std::string, std::chrono::steady_clock::time_point> startTime_;
    mutable std::mutex mutex_;
};

/**
 * @brief 回调观察者
 * 
 * 将工作流事件转发到用户提供的回调函数
 */
class CallbackObserver : public IWorkflowObserver {
public:
    using StartedCallback = std::function<void(const std::string&)>;
    using FinishedCallback = std::function<void(const std::string&)>;
    using InterruptedCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&, const std::string&)>;

    CallbackObserver(StartedCallback started = nullptr,
                  FinishedCallback finished = nullptr,
                  InterruptedCallback interrupted = nullptr,
                  ErrorCallback error = nullptr)
        : startedCallback_(started),
          finishedCallback_(finished),
          interruptedCallback_(interrupted),
          errorCallback_(error) {}

    void onWorkflowStarted(const std::string& workflowName) override {
        if (startedCallback_) {
            startedCallback_(workflowName);
        }
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        if (finishedCallback_) {
            finishedCallback_(workflowName);
        }
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        if (interruptedCallback_) {
            interruptedCallback_(workflowName);
        }
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        if (errorCallback_) {
            errorCallback_(workflowName, error);
        }
    }

private:
    StartedCallback startedCallback_;
    FinishedCallback finishedCallback_;
    InterruptedCallback interruptedCallback_;
    ErrorCallback errorCallback_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_OBSERVER_IMPL_H
