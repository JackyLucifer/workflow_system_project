/**
 * @file metrics_collector.h
 * @brief 业务流程管理系统 - 指标收集器实现
 *
 * 设计模式：
 * - 单例模式：全局唯一的指标收集器
 * - 观察者模式：收集工作流执行指标
 *
 * 面向对象：
 * - 封装：封装指标收集、统计和报告生成逻辑
 */

#ifndef WORKFLOW_SYSTEM_METRICS_COLLECTOR_IMPL_H
#define WORKFLOW_SYSTEM_METRICS_COLLECTOR_IMPL_H

#include "workflow_system/interfaces/metrics_collector.h"
#include <mutex>
#include <map>
#include <vector>

namespace WorkflowSystem {

/**
 * @brief 指标收集器实现
 */
class MetricsCollector : public IMetricsCollector {
public:
    static MetricsCollector& getInstance();

    void recordWorkflowStart(const std::string& workflowName) override;
    void recordWorkflowEnd(const std::string& workflowName, bool success,
                           const std::string& errorMessage) override;
    void recordMessage(const std::string& workflowName,
                       const std::string& messageType) override;
    void recordInterrupt(const std::string& workflowName) override;
    void recordPause(const std::string& workflowName) override;
    void recordResume(const std::string& workflowName) override;

    std::vector<WorkflowMetrics> getWorkflowMetrics(
        const std::string& workflowName) const override;
    std::vector<WorkflowMetrics> getAllMetrics() const override;

    WorkflowStatistics getStatistics(const std::string& workflowName) const override;
    std::map<std::string, WorkflowStatistics> getAllStatistics() const override;

    std::string generateReport(MetricsReportType type) const override;
    std::string generateWorkflowReport(const std::string& workflowName,
                                      MetricsReportType type) const override;

    void clearMetrics(const std::string& workflowName) override;
    bool exportMetrics(const std::string& filePath,
                       MetricsReportType type) const override;

private:
    MetricsCollector() = default;

    // 删除拷贝构造和赋值
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    // 生成各种报告
    std::string generateSummaryReport() const;
    std::string generateDetailedReport() const;
    std::string generateJsonReport() const;
    std::string generateCsvReport() const;
    std::string generateWorkflowSummaryReport(const std::string& workflowName,
                                             const std::vector<WorkflowMetrics>& metrics) const;
    std::string generateWorkflowDetailedReport(const std::string& workflowName,
                                                const std::vector<WorkflowMetrics>& metrics) const;
    std::string generateWorkflowJsonReport(const std::string& workflowName,
                                          const std::vector<WorkflowMetrics>& metrics) const;
    std::string generateWorkflowCsvReport(const std::string& workflowName,
                                         const std::vector<WorkflowMetrics>& metrics) const;

    mutable std::recursive_mutex mutex_;
    std::map<std::string, std::vector<WorkflowMetrics>> metricsHistory_;
    std::map<std::string, WorkflowMetrics> activeMetrics_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_METRICS_COLLECTOR_IMPL_H
