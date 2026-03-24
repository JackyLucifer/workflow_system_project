/**
 * @file metrics_collector.h
 * @brief 业务流程管理系统 - 指标收集器接口
 *
 * 设计模式：
 * - 观察者模式：收集工作流执行指标
 * - 单例模式：全局指标收集
 *
 * 面向对象：
 * - 封装：封装指标收集和统计逻辑
 */

#ifndef WORKFLOW_SYSTEM_METRICS_COLLECTOR_H
#define WORKFLOW_SYSTEM_METRICS_COLLECTOR_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace WorkflowSystem {

/**
 * @brief 工作流指标
 */
struct WorkflowMetrics {
    std::string workflowName;          // 工作流名称
    int64_t startTime;                 // 开始时间（毫秒）
    int64_t endTime;                   // 结束时间（毫秒）
    int64_t duration;                  // 执行时长（毫秒）
    std::string startState;            // 开始状态
    std::string endState;              // 结束状态
    bool success;                      // 是否成功
    std::string errorMessage;          // 错误消息
    int messageCount;                  // 处理的消息数量
    int interruptCount;                // 中断次数
    int pauseCount;                    // 暂停次数
    int resumeCount;                   // 恢复次数

    WorkflowMetrics()
        : startTime(0), endTime(0), duration(0),
          success(false), messageCount(0),
          interruptCount(0), pauseCount(0), resumeCount(0) {}
};

/**
 * @brief 工作流统计摘要
 */
struct WorkflowStatistics {
    int totalExecutions;               // 总执行次数
    int successCount;                  // 成功次数
    int failureCount;                  // 失败次数
    int64_t totalDuration;             // 总执行时长（毫秒）
    int64_t minDuration;              // 最短执行时长（毫秒）
    int64_t maxDuration;              // 最长执行时长（毫秒）
    double averageDuration;            // 平均执行时长（毫秒）
    double successRate;               // 成功率（0-1）
    int64_t totalMessages;            // 处理消息总数

    WorkflowStatistics()
        : totalExecutions(0), successCount(0), failureCount(0),
          totalDuration(0), minDuration(INT64_MAX), maxDuration(0),
          averageDuration(0.0), successRate(0.0), totalMessages(0) {}
};

/**
 * @brief 指标报告类型
 */
enum class MetricsReportType {
    SUMMARY,              // 摘要报告
    DETAILED,            // 详细报告
    JSON,                // JSON 格式报告
    CSV                  // CSV 格式报告
};

/**
 * @brief 指标收集器接口
 */
class IMetricsCollector {
public:
    virtual ~IMetricsCollector() = default;

    /**
     * @brief 记录工作流启动
     */
    virtual void recordWorkflowStart(const std::string& workflowName) = 0;

    /**
     * @brief 记录工作流完成
     */
    virtual void recordWorkflowEnd(const std::string& workflowName, bool success,
                                   const std::string& errorMessage = "") = 0;

    /**
     * @brief 记录消息处理
     */
    virtual void recordMessage(const std::string& workflowName,
                             const std::string& messageType) = 0;

    /**
     * @brief 记录中断
     */
    virtual void recordInterrupt(const std::string& workflowName) = 0;

    /**
     * @brief 记录暂停
     */
    virtual void recordPause(const std::string& workflowName) = 0;

    /**
     * @brief 记录恢复
     */
    virtual void recordResume(const std::string& workflowName) = 0;

    /**
     * @brief 获取工作流指标
     */
    virtual std::vector<WorkflowMetrics> getWorkflowMetrics(
        const std::string& workflowName) const = 0;

    /**
     * @brief 获取所有工作流指标
     */
    virtual std::vector<WorkflowMetrics> getAllMetrics() const = 0;

    /**
     * @brief 获取工作流统计摘要
     */
    virtual WorkflowStatistics getStatistics(
        const std::string& workflowName) const = 0;

    /**
     * @brief 获取所有工作流统计摘要
     */
    virtual std::map<std::string, WorkflowStatistics> getAllStatistics() const = 0;

    /**
     * @brief 生成报告
     */
    virtual std::string generateReport(
        MetricsReportType type = MetricsReportType::SUMMARY) const = 0;

    /**
     * @brief 生成特定工作流的报告
     */
    virtual std::string generateWorkflowReport(
        const std::string& workflowName,
        MetricsReportType type = MetricsReportType::SUMMARY) const = 0;

    /**
     * @brief 清除指标数据
     */
    virtual void clearMetrics(const std::string& workflowName = "") = 0;

    /**
     * @brief 导出指标到文件
     */
    virtual bool exportMetrics(const std::string& filePath,
                             MetricsReportType type = MetricsReportType::JSON) const = 0;
};

/**
 * @brief 指标回调函数
 */
using MetricsCallback = std::function<void(const WorkflowMetrics&)>;

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_METRICS_COLLECTOR_H
