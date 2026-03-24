/**
 * @file workflow_scheduler_impl.h
 * @brief 工作流调度器实现
 *
 * 功能：定时触发、周期执行、多线程调度
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_IMPL_H

#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>
#include "interfaces/workflow_scheduler.h"
#include "core/logger.h"
#include "core/utils.h"

namespace WorkflowSystem {

/**
 * @brief 调度任务内部结构
 */
struct ScheduledTask {
    ScheduleConfig config;
    int64_t nextExecutionTime;
    int executionCount;
    ScheduleState state;
    
    ScheduledTask() : nextExecutionTime(0), executionCount(0), state(ScheduleState::PENDING) {}
    
    bool operator<(const ScheduledTask& other) const {
        return nextExecutionTime > other.nextExecutionTime;
    }
};

/**
 * @brief 工作流调度器实现
 *
 * 使用优先队列管理定时任务，支持多种调度策略
 */
class WorkflowScheduler : public IWorkflowScheduler {
public:
    WorkflowScheduler() 
        : running_(false)
        , schedulerThread_()
        , mutex_()
        , cv_() {}
    
    ~WorkflowScheduler() {
        stop();
    }
    
    // ========== 调度管理 ==========
    
    std::string schedule(const ScheduleConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string scheduleId = config.scheduleId;
        if (scheduleId.empty()) {
            scheduleId = "sched_" + IdGenerator::generate();
        }
        
        ScheduledTask task;
        task.config = config;
        task.config.scheduleId = scheduleId;
        task.executionCount = 0;
        task.state = config.enabled ? ScheduleState::PENDING : ScheduleState::PAUSED;
        
        // 计算下次执行时间
        task.nextExecutionTime = calculateNextExecution(task.config, 0);
        
        tasks_[scheduleId] = task;
        addToPriorityQueue(scheduleId, task.nextExecutionTime);
        
        LOG_INFO("[WorkflowScheduler] Scheduled: " + scheduleId + 
                 " for workflow: " + config.workflowName);
        
        cv_.notify_one();
        return scheduleId;
    }
    
    bool cancelSchedule(const std::string& scheduleId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(scheduleId);
        if (it == tasks_.end()) {
            return false;
        }
        
        it->second.state = ScheduleState::CANCELLED;
        tasks_.erase(it);
        
        LOG_INFO("[WorkflowScheduler] Cancelled: " + scheduleId);
        return true;
    }
    
    bool pauseSchedule(const std::string& scheduleId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(scheduleId);
        if (it == tasks_.end()) {
            return false;
        }
        
        if (it->second.state == ScheduleState::RUNNING) {
            return false;
        }
        
        it->second.state = ScheduleState::PAUSED;
        LOG_INFO("[WorkflowScheduler] Paused: " + scheduleId);
        return true;
    }
    
    bool resumeSchedule(const std::string& scheduleId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(scheduleId);
        if (it == tasks_.end()) {
            return false;
        }
        
        if (it->second.state != ScheduleState::PAUSED) {
            return false;
        }
        
        it->second.state = ScheduleState::PENDING;
        it->second.nextExecutionTime = calculateNextExecution(it->second.config, 0);
        
        LOG_INFO("[WorkflowScheduler] Resumed: " + scheduleId);
        cv_.notify_one();
        return true;
    }
    
    bool updateSchedule(const std::string& scheduleId, const ScheduleConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(scheduleId);
        if (it == tasks_.end()) {
            return false;
        }
        
        it->second.config = config;
        it->second.config.scheduleId = scheduleId;
        it->second.nextExecutionTime = calculateNextExecution(it->second.config, it->second.executionCount);
        
        LOG_INFO("[WorkflowScheduler] Updated: " + scheduleId);
        cv_.notify_one();
        return true;
    }
    
    // ========== 查询接口 ==========
    
    ScheduleConfig getScheduleConfig(const std::string& scheduleId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(scheduleId);
        if (it != tasks_.end()) {
            return it->second.config;
        }
        return ScheduleConfig();
    }
    
    std::vector<ScheduleConfig> getAllSchedules() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ScheduleConfig> result;
        for (const auto& pair : tasks_) {
            result.push_back(pair.second.config);
        }
        return result;
    }
    
    std::vector<ScheduleConfig> getSchedulesByWorkflow(const std::string& workflowName) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ScheduleConfig> result;
        for (const auto& pair : tasks_) {
            if (pair.second.config.workflowName == workflowName) {
                result.push_back(pair.second.config);
            }
        }
        return result;
    }
    
    std::vector<ScheduleRecord> getScheduleRecords(const std::string& scheduleId, int limit) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = records_.find(scheduleId);
        if (it == records_.end()) {
            return std::vector<ScheduleRecord>();
        }
        
        std::vector<ScheduleRecord> result;
        int count = 0;
        for (auto rit = it->second.rbegin(); rit != it->second.rend() && count < limit; ++rit, ++count) {
            result.push_back(*rit);
        }
        return result;
    }
    
    // ========== 调度器控制 ==========
    
    void start() override {
        if (running_.exchange(true)) {
            return;
        }
        
        schedulerThread_ = std::thread(&WorkflowScheduler::schedulerLoop, this);
        LOG_INFO("[WorkflowScheduler] Started");
    }
    
    void stop() override {
        if (!running_.exchange(false)) {
            return;
        }
        
        cv_.notify_all();
        if (schedulerThread_.joinable()) {
            schedulerThread_.join();
        }
        
        LOG_INFO("[WorkflowScheduler] Stopped");
    }
    
    bool isRunning() const override {
        return running_;
    }
    
    void setExecutionCallback(ScheduleCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = callback;
    }
    
    // ========== 下次执行时间 ==========
    
    int64_t getNextExecutionTime(const std::string& scheduleId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tasks_.find(scheduleId);
        if (it != tasks_.end()) {
            return it->second.nextExecutionTime;
        }
        return 0;
    }
    
    std::vector<std::pair<std::string, int64_t>> getAllNextExecutionTimes() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, int64_t>> result;
        for (const auto& pair : tasks_) {
            if (pair.second.state == ScheduleState::PENDING) {
                result.push_back({pair.first, pair.second.nextExecutionTime});
            }
        }
        
        std::sort(result.begin(), result.end(), 
            [](const std::pair<std::string, int64_t>& a, 
               const std::pair<std::string, int64_t>& b) { 
                return a.second < b.second; 
            });
        
        return result;
    }

private:
    std::atomic<bool> running_;
    std::thread schedulerThread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    std::unordered_map<std::string, ScheduledTask> tasks_;
    std::unordered_map<std::string, std::vector<ScheduleRecord>> records_;
    std::priority_queue<std::pair<int64_t, std::string>> priorityQueue_;
    
    ScheduleCallback callback_;
    
    void addToPriorityQueue(const std::string& scheduleId, int64_t executionTime) {
        priorityQueue_.push({executionTime, scheduleId});
    }
    
    void schedulerLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            int64_t now = TimeUtils::getCurrentMs();
            int64_t nextWakeTime = now + 60000;
            
            // 检查是否有需要执行的任务
            while (!priorityQueue_.empty()) {
                auto top = priorityQueue_.top();
                
                if (top.first > now) {
                    nextWakeTime = top.first;
                    break;
                }
                
                priorityQueue_.pop();
                
                auto it = tasks_.find(top.second);
                if (it == tasks_.end() || it->second.state != ScheduleState::PENDING) {
                    continue;
                }
                
                // 执行任务
                executeTask(it->second);
                
                // 重新调度
                if (shouldReschedule(it->second)) {
                    it->second.nextExecutionTime = calculateNextExecution(
                        it->second.config, it->second.executionCount);
                    priorityQueue_.push({it->second.nextExecutionTime, top.second});
                }
            }
            
            // 等待下一次执行或被唤醒
            cv_.wait_for(lock, std::chrono::milliseconds(nextWakeTime - now),
                [this]() { return !running_; });
        }
    }
    
    void executeTask(ScheduledTask& task) {
        task.state = ScheduleState::RUNNING;
        task.executionCount++;
        
        ScheduleRecord record;
        record.scheduleId = task.config.scheduleId;
        record.executionId = "exec_" + IdGenerator::generate();
        record.scheduledTime = task.nextExecutionTime;
        record.actualTime = TimeUtils::getCurrentMs();
        record.executionCount = task.executionCount;
        
        LOG_INFO("[WorkflowScheduler] Executing: " + task.config.scheduleId + 
                 " (count: " + std::to_string(task.executionCount) + ")");
        
        try {
            // 调用回调执行工作流
            if (callback_) {
                callback_(record);
            }
            
            record.state = ScheduleState::COMPLETED;
            task.state = ScheduleState::PENDING;
            
        } catch (const std::exception& e) {
            record.state = ScheduleState::FAILED;
            record.errorMessage = e.what();
            task.state = ScheduleState::FAILED;
            
            LOG_ERROR("[WorkflowScheduler] Execution failed: " + std::string(e.what()));
        }
        
        // 记录执行历史
        records_[task.config.scheduleId].push_back(record);
        
        // 限制历史记录数量
        auto& history = records_[task.config.scheduleId];
        if (history.size() > 1000) {
            history.erase(history.begin());
        }
    }
    
    bool shouldReschedule(const ScheduledTask& task) {
        if (!task.config.enabled) {
            return false;
        }
        
        if (task.config.type == ScheduleType::ONCE) {
            return false;
        }
        
        if (task.config.maxExecutions > 0 && task.executionCount >= task.config.maxExecutions) {
            return false;
        }
        
        if (task.config.endTime > 0) {
            int64_t nextTime = calculateNextExecution(task.config, task.executionCount);
            if (nextTime > task.config.endTime) {
                return false;
            }
        }
        
        return true;
    }
    
    int64_t calculateNextExecution(const ScheduleConfig& config, int executionCount) {
        int64_t now = TimeUtils::getCurrentMs();
        int64_t baseTime = config.startTime > 0 ? config.startTime : now;
        
        switch (config.type) {
            case ScheduleType::ONCE:
                return baseTime;
                
            case ScheduleType::INTERVAL:
                if (executionCount == 0) {
                    return baseTime;
                }
                return now + config.intervalMs;
                
            case ScheduleType::DAILY:
                return now + 24 * 60 * 60 * 1000;
                
            case ScheduleType::WEEKLY:
                return now + 7 * 24 * 60 * 60 * 1000;
                
            case ScheduleType::MONTHLY:
                return now + 30 * 24 * 60 * 60 * 1000;
                
            case ScheduleType::CRON:
                return parseCronExpression(config.cronExpression, now);
                
            default:
                return now;
        }
    }
    
    int64_t parseCronExpression(const std::string& cronExpr, int64_t fromTime) {
        (void)cronExpr;
        (void)fromTime;
        return fromTime + 60000;
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_IMPL_H
