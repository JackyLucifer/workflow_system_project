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
#include "workflow_system/interfaces/workflow_scheduler.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"

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

    std::string schedule(const ScheduleConfig& config) override;
    bool cancelSchedule(const std::string& scheduleId) override;
    bool pauseSchedule(const std::string& scheduleId) override;
    bool resumeSchedule(const std::string& scheduleId) override;
    bool updateSchedule(const std::string& scheduleId, const ScheduleConfig& config) override;

    // ========== 查询接口 ==========

    ScheduleConfig getScheduleConfig(const std::string& scheduleId) const override;
    std::vector<ScheduleConfig> getAllSchedules() const override;
    std::vector<ScheduleConfig> getSchedulesByWorkflow(const std::string& workflowName) const override;
    std::vector<ScheduleRecord> getScheduleRecords(const std::string& scheduleId, int limit) const override;

    // ========== 调度器控制 ==========

    void start() override;
    void stop() override;
    bool isRunning() const override;
    void setExecutionCallback(ScheduleCallback callback) override;

    // ========== 下次执行时间 ==========

    int64_t getNextExecutionTime(const std::string& scheduleId) const override;
    std::vector<std::pair<std::string, int64_t>> getAllNextExecutionTimes() const override;

private:
    std::atomic<bool> running_;
    std::thread schedulerThread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::unordered_map<std::string, ScheduledTask> tasks_;
    std::unordered_map<std::string, std::vector<ScheduleRecord>> records_;
    std::priority_queue<std::pair<int64_t, std::string>> priorityQueue_;

    ScheduleCallback callback_;

    void addToPriorityQueue(const std::string& scheduleId, int64_t executionTime);
    void schedulerLoop();
    void executeTask(ScheduledTask& task);
    bool shouldReschedule(const ScheduledTask& task);
    int64_t calculateNextExecution(const ScheduleConfig& config, int executionCount);
    int64_t parseCronExpression(const std::string& cronExpr, int64_t fromTime);
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_IMPL_H
