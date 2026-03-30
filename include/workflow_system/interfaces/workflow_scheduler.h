/**
 * @file workflow_scheduler.h
 * @brief 工作流调度器接口
 *
 * 设计模式：策略模式 - 支持多种调度策略
 * 功能：定时触发、周期执行、Cron表达式调度
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_H
#define WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_H

#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <vector>
#include "workflow_system/core/types.h"

namespace WorkflowSystem {

/**
 * @brief 调度类型枚举
 */
enum class ScheduleType {
    ONCE,           // 执行一次
    INTERVAL,       // 固定间隔
    DAILY,          // 每天执行
    WEEKLY,         // 每周执行
    MONTHLY,        // 每月执行
    CRON            // Cron表达式
};

/**
 * @brief 调度状态
 */
enum class ScheduleState {
    PENDING,        // 等待中
    RUNNING,        // 运行中
    PAUSED,         // 已暂停
    COMPLETED,      // 已完成
    FAILED,         // 失败
    CANCELLED       // 已取消
};

/**
 * @brief 调度配置
 */
struct ScheduleConfig {
    std::string scheduleId;             // 调度ID
    std::string workflowName;           // 工作流名称
    ScheduleType type;                  // 调度类型
    int64_t intervalMs;                 // 间隔时间（毫秒）
    std::string cronExpression;         // Cron表达式
    int64_t startTime;                  // 开始时间戳
    int64_t endTime;                    // 结束时间戳（0表示无限制）
    int maxExecutions;                  // 最大执行次数（0表示无限制）
    bool enabled;                       // 是否启用
    std::string description;            // 描述
    
    ScheduleConfig()
        : type(ScheduleType::ONCE)
        , intervalMs(0)
        , startTime(0)
        , endTime(0)
        , maxExecutions(0)
        , enabled(true) {}
};

/**
 * @brief 调度记录
 */
struct ScheduleRecord {
    std::string scheduleId;
    std::string executionId;
    int64_t scheduledTime;              // 计划执行时间
    int64_t actualTime;                 // 实际执行时间
    ScheduleState state;
    std::string errorMessage;
    int executionCount;                 // 已执行次数
    
    ScheduleRecord()
        : scheduledTime(0)
        , actualTime(0)
        , state(ScheduleState::PENDING)
        , executionCount(0) {}
};

/**
 * @brief 调度回调类型
 */
using ScheduleCallback = std::function<void(const ScheduleRecord& record)>;

/**
 * @brief 工作流调度器接口
 *
 * 提供定时触发和周期执行能力
 */
class IWorkflowScheduler {
public:
    virtual ~IWorkflowScheduler() = default;

    // ========== 调度管理 ==========
    
    /**
     * @brief 添加调度任务
     * @param config 调度配置
     * @return 调度ID
     */
    virtual std::string schedule(const ScheduleConfig& config) = 0;
    
    /**
     * @brief 取消调度
     * @param scheduleId 调度ID
     * @return 是否成功
     */
    virtual bool cancelSchedule(const std::string& scheduleId) = 0;
    
    /**
     * @brief 暂停调度
     * @param scheduleId 调度ID
     * @return 是否成功
     */
    virtual bool pauseSchedule(const std::string& scheduleId) = 0;
    
    /**
     * @brief 恢复调度
     * @param scheduleId 调度ID
     * @return 是否成功
     */
    virtual bool resumeSchedule(const std::string& scheduleId) = 0;
    
    /**
     * @brief 更新调度配置
     * @param scheduleId 调度ID
     * @param config 新配置
     * @return 是否成功
     */
    virtual bool updateSchedule(const std::string& scheduleId, const ScheduleConfig& config) = 0;
    
    // ========== 查询接口 ==========
    
    /**
     * @brief 获取调度配置
     * @param scheduleId 调度ID
     * @return 调度配置
     */
    virtual ScheduleConfig getScheduleConfig(const std::string& scheduleId) const = 0;
    
    /**
     * @brief 获取所有调度
     * @return 调度列表
     */
    virtual std::vector<ScheduleConfig> getAllSchedules() const = 0;
    
    /**
     * @brief 获取工作流的所有调度
     * @param workflowName 工作流名称
     * @return 调度列表
     */
    virtual std::vector<ScheduleConfig> getSchedulesByWorkflow(const std::string& workflowName) const = 0;
    
    /**
     * @brief 获取调度记录
     * @param scheduleId 调度ID
     * @param limit 限制数量
     * @return 记录列表
     */
    virtual std::vector<ScheduleRecord> getScheduleRecords(const std::string& scheduleId, int limit = 100) const = 0;
    
    // ========== 调度器控制 ==========
    
    /**
     * @brief 启动调度器
     */
    virtual void start() = 0;
    
    /**
     * @brief 停止调度器
     */
    virtual void stop() = 0;
    
    /**
     * @brief 检查调度器是否运行中
     * @return 是否运行中
     */
    virtual bool isRunning() const = 0;
    
    /**
     * @brief 设置执行回调
     * @param callback 回调函数
     */
    virtual void setExecutionCallback(ScheduleCallback callback) = 0;
    
    // ========== 下次执行时间 ==========
    
    /**
     * @brief 获取下次执行时间
     * @param scheduleId 调度ID
     * @return 时间戳（毫秒），0表示无下次执行
     */
    virtual int64_t getNextExecutionTime(const std::string& scheduleId) const = 0;
    
    /**
     * @brief 获取所有下次执行时间
     * @return 调度ID到时间的映射
     */
    virtual std::vector<std::pair<std::string, int64_t>> getAllNextExecutionTimes() const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_SCHEDULER_H
