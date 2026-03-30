/**
 * @file workflow_tracer.h
 * @brief 工作流追踪器 - 执行路径记录和诊断
 *
 * 功能：追踪工作流执行路径、性能分析、问题诊断
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_TRACER_H
#define WORKFLOW_SYSTEM_WORKFLOW_TRACER_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include "workflow_system/core/types.h"
#include "workflow_system/core/any.h"

namespace WorkflowSystem {

/**
 * @brief 追踪事件类型
 */
enum class TraceEventType {
    WORKFLOW_STARTED,       // 工作流启动
    WORKFLOW_COMPLETED,     // 工作流完成
    WORKFLOW_FAILED,        // 工作流失败
    WORKFLOW_PAUSED,        // 工作流暂停
    WORKFLOW_RESUMED,       // 工作流恢复
    NODE_STARTED,           // 节点启动
    NODE_COMPLETED,         // 节点完成
    NODE_FAILED,            // 节点失败
    MESSAGE_RECEIVED,       // 消息接收
    STATE_CHANGED,          // 状态变更
    ERROR_OCCURRED,         // 错误发生
    RETRY_ATTEMPT,          // 重试尝试
    TIMEOUT_OCCURRED,       // 超时发生
    RESOURCE_CREATED,       // 资源创建
    RESOURCE_RELEASED       // 资源释放
};

/**
 * @brief 追踪事件
 */
struct TraceEvent {
    std::string traceId;            // 追踪ID
    std::string workflowName;       // 工作流名称
    std::string nodeId;             // 节点ID（可选）
    TraceEventType type;            // 事件类型
    int64_t timestamp;              // 时间戳
    int64_t duration;               // 持续时间（毫秒）
    WorkflowState fromState;        // 原状态
    WorkflowState toState;          // 新状态
    std::string message;            // 消息
    std::string error;              // 错误信息
    std::unordered_map<std::string, std::string> metadata;  // 元数据
    
    TraceEvent()
        : timestamp(0)
        , duration(0)
        , fromState(WorkflowState::IDLE)
        , toState(WorkflowState::IDLE) {}
};

/**
 * @brief 执行追踪信息
 */
struct ExecutionTrace {
    std::string traceId;            // 追踪ID
    std::string workflowName;       // 工作流名称
    int64_t startTime;              // 开始时间
    int64_t endTime;                // 结束时间
    int64_t totalDuration;          // 总持续时间
    WorkflowState finalState;       // 最终状态
    std::vector<TraceEvent> events; // 事件列表
    int eventCount;                 // 事件数量
    int errorCount;                 // 错误数量
    int retryCount;                 // 重试次数
    
    ExecutionTrace()
        : startTime(0)
        , endTime(0)
        , totalDuration(0)
        , finalState(WorkflowState::IDLE)
        , eventCount(0)
        , errorCount(0)
        , retryCount(0) {}
};

/**
 * @brief 追踪统计
 */
struct TraceStatistics {
    std::string workflowName;
    int totalExecutions;            // 总执行次数
    int successCount;               // 成功次数
    int failureCount;               // 失败次数
    int64_t totalDuration;          // 总耗时
    int64_t avgDuration;            // 平均耗时
    int64_t minDuration;            // 最小耗时
    int64_t maxDuration;            // 最大耗时
    double successRate;             // 成功率
    
    TraceStatistics()
        : totalExecutions(0)
        , successCount(0)
        , failureCount(0)
        , totalDuration(0)
        , avgDuration(0)
        , minDuration(0)
        , maxDuration(0)
        , successRate(0.0) {}
};

/**
 * @brief 工作流追踪器接口
 */
class IWorkflowTracer {
public:
    virtual ~IWorkflowTracer() = default;
    
    // ========== 追踪控制 ==========
    
    /**
     * @brief 开始追踪
     * @param workflowName 工作流名称
     * @return 追踪ID
     */
    virtual std::string startTrace(const std::string& workflowName) = 0;
    
    /**
     * @brief 结束追踪
     * @param traceId 追踪ID
     * @param finalState 最终状态
     */
    virtual void endTrace(const std::string& traceId, WorkflowState finalState) = 0;
    
    /**
     * @brief 记录事件
     * @param traceId 追踪ID
     * @param event 事件
     */
    virtual void recordEvent(const std::string& traceId, const TraceEvent& event) = 0;
    
    // ========== 便捷记录方法 ==========
    
    /**
     * @brief 记录状态变更
     */
    virtual void recordStateChange(const std::string& traceId,
                                   WorkflowState fromState,
                                   WorkflowState toState) = 0;
    
    /**
     * @brief 记录错误
     */
    virtual void recordError(const std::string& traceId,
                            const std::string& errorMessage) = 0;
    
    /**
     * @brief 记录节点执行
     */
    virtual void recordNodeExecution(const std::string& traceId,
                                    const std::string& nodeId,
                                    bool success,
                                    int64_t duration) = 0;
    
    // ========== 查询接口 ==========
    
    /**
     * @brief 获取追踪信息
     * @param traceId 追踪ID
     * @return 追踪信息
     */
    virtual ExecutionTrace getTrace(const std::string& traceId) const = 0;
    
    /**
     * @brief 获取工作流的所有追踪
     * @param workflowName 工作流名称
     * @param limit 限制数量
     * @return 追踪列表
     */
    virtual std::vector<ExecutionTrace> getTracesByWorkflow(
        const std::string& workflowName, int limit = 100) const = 0;
    
    /**
     * @brief 获取活跃追踪
     * @return 活跃追踪列表
     */
    virtual std::vector<ExecutionTrace> getActiveTraces() const = 0;
    
    /**
     * @brief 获取追踪统计
     * @param workflowName 工作流名称
     * @return 统计信息
     */
    virtual TraceStatistics getStatistics(const std::string& workflowName) const = 0;
    
    // ========== 管理接口 ==========
    
    /**
     * @brief 清除追踪历史
     * @param workflowName 工作流名称（空表示全部）
     */
    virtual void clearTraces(const std::string& workflowName = "") = 0;
    
    /**
     * @brief 设置最大追踪数量
     * @param maxTraces 最大数量
     */
    virtual void setMaxTraces(int maxTraces) = 0;
    
    /**
     * @brief 导出追踪数据
     * @param format 格式（json/csv）
     * @return 导出数据
     */
    virtual std::string exportTraces(const std::string& format = "json") const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_TRACER_H
