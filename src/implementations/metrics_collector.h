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

#include "interfaces/metrics_collector.h"
#include "core/logger.h"
#include "core/utils.h"
#include <mutex>
#include <map>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <climits>

namespace WorkflowSystem {

/**
 * @brief 指标收集器实现
 */
class MetricsCollector : public IMetricsCollector {
public:
    static MetricsCollector& getInstance() {
        static MetricsCollector instance;
        return instance;
    }

    void recordWorkflowStart(const std::string& workflowName) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        WorkflowMetrics metrics;
        metrics.workflowName = workflowName;
        metrics.startTime = TimeUtils::getCurrentMs();
        metrics.startState = "started";

        activeMetrics_[workflowName] = metrics;

        LOG_INFO("[Metrics] Recording workflow start: " + workflowName);
    }

    void recordWorkflowEnd(const std::string& workflowName, bool success,
                           const std::string& errorMessage) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = activeMetrics_.find(workflowName);
        if (it == activeMetrics_.end()) {
            LOG_WARNING("[Metrics] No active metrics found for workflow: " + workflowName);
            return;
        }

        WorkflowMetrics metrics = it->second;
        metrics.endTime = TimeUtils::getCurrentMs();
        metrics.duration = metrics.endTime - metrics.startTime;
        metrics.success = success;
        metrics.errorMessage = errorMessage;
        metrics.endState = success ? "completed" : "failed";

        // 保存到历史记录
        metricsHistory_[workflowName].push_back(metrics);

        // 限制历史记录数量（最多保留100条）
        if (metricsHistory_[workflowName].size() > 100) {
            metricsHistory_[workflowName].erase(metricsHistory_[workflowName].begin());
        }

        // 从活动指标中移除
        activeMetrics_.erase(it);

        LOG_INFO("[Metrics] Recording workflow end: " + workflowName +
                  " (success: " + (success ? "true" : "false") +
                  ", duration: " + std::to_string(metrics.duration) + "ms)");
    }

    void recordMessage(const std::string& workflowName,
                       const std::string& /*messageType*/) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = activeMetrics_.find(workflowName);
        if (it != activeMetrics_.end()) {
            it->second.messageCount++;
        }
    }

    void recordInterrupt(const std::string& workflowName) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = activeMetrics_.find(workflowName);
        if (it != activeMetrics_.end()) {
            it->second.interruptCount++;
        }
    }

    void recordPause(const std::string& workflowName) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = activeMetrics_.find(workflowName);
        if (it != activeMetrics_.end()) {
            it->second.pauseCount++;
        }
    }

    void recordResume(const std::string& workflowName) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = activeMetrics_.find(workflowName);
        if (it != activeMetrics_.end()) {
            it->second.resumeCount++;
        }
    }

    std::vector<WorkflowMetrics> getWorkflowMetrics(
        const std::string& workflowName) const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = metricsHistory_.find(workflowName);
        if (it != metricsHistory_.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<WorkflowMetrics> getAllMetrics() const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        std::vector<WorkflowMetrics> all;
        for (const auto& pair : metricsHistory_) {
            all.insert(all.end(), pair.second.begin(), pair.second.end());
        }
        return all;
    }

    WorkflowStatistics getStatistics(const std::string& workflowName) const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        WorkflowStatistics stats;

        auto it = metricsHistory_.find(workflowName);
        if (it == metricsHistory_.end()) {
            return stats;
        }

        const auto& metrics = it->second;
        stats.totalExecutions = static_cast<int>(metrics.size());

        for (const auto& m : metrics) {
            if (m.success) {
                stats.successCount++;
            } else {
                stats.failureCount++;
            }

            stats.totalDuration += m.duration;
            stats.minDuration = std::min(stats.minDuration, m.duration);
            stats.maxDuration = std::max(stats.maxDuration, m.duration);
            stats.totalMessages += m.messageCount;
        }

        if (stats.totalExecutions > 0) {
            stats.averageDuration = static_cast<double>(stats.totalDuration) / stats.totalExecutions;
            stats.successRate = static_cast<double>(stats.successCount) / stats.totalExecutions;
        }

        return stats;
    }

    std::map<std::string, WorkflowStatistics> getAllStatistics() const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        std::map<std::string, WorkflowStatistics> allStats;

        for (const auto& pair : metricsHistory_) {
            allStats[pair.first] = getStatistics(pair.first);
        }

        return allStats;
    }

    std::string generateReport(MetricsReportType type) const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        switch (type) {
            case MetricsReportType::SUMMARY:
                return generateSummaryReport();
            case MetricsReportType::DETAILED:
                return generateDetailedReport();
            case MetricsReportType::JSON:
                return generateJsonReport();
            case MetricsReportType::CSV:
                return generateCsvReport();
            default:
                return generateSummaryReport();
        }
    }

    std::string generateWorkflowReport(const std::string& workflowName,
                                      MetricsReportType type) const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        auto it = metricsHistory_.find(workflowName);
        if (it == metricsHistory_.end()) {
            return "No metrics found for workflow: " + workflowName;
        }

        const auto& metrics = it->second;

        switch (type) {
            case MetricsReportType::SUMMARY:
                return generateWorkflowSummaryReport(workflowName, metrics);
            case MetricsReportType::DETAILED:
                return generateWorkflowDetailedReport(workflowName, metrics);
            case MetricsReportType::JSON:
                return generateWorkflowJsonReport(workflowName, metrics);
            case MetricsReportType::CSV:
                return generateWorkflowCsvReport(workflowName, metrics);
            default:
                return generateWorkflowSummaryReport(workflowName, metrics);
        }
    }

    void clearMetrics(const std::string& workflowName) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        if (workflowName.empty()) {
            metricsHistory_.clear();
            activeMetrics_.clear();
            LOG_INFO("[Metrics] All metrics cleared");
        } else {
            metricsHistory_.erase(workflowName);
            activeMetrics_.erase(workflowName);
            LOG_INFO("[Metrics] Metrics cleared for workflow: " + workflowName);
        }
    }

    bool exportMetrics(const std::string& filePath,
                       MetricsReportType type) const override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("[Metrics] Failed to open file for export: " + filePath);
            return false;
        }

        file << generateReport(type);
        file.close();

        LOG_INFO("[Metrics] Metrics exported to: " + filePath);
        return true;
    }

private:
    MetricsCollector() = default;

    // 删除拷贝构造和赋值
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    // 生成摘要报告
    std::string generateSummaryReport() const {
        std::ostringstream oss;
        oss << "========== Workflow Metrics Summary ==========\n\n";

        auto allStats = getAllStatistics();

        if (allStats.empty()) {
            oss << "No metrics data available.\n";
            return oss.str();
        }

        int totalExecutions = 0;
        int totalSuccesses = 0;
        int64_t totalDuration = 0;

        for (const auto& pair : allStats) {
            const auto& stats = pair.second;
            totalExecutions += stats.totalExecutions;
            totalSuccesses += stats.successCount;
            totalDuration += stats.totalDuration;

            oss << "Workflow: " << pair.first << "\n";
            oss << "  Executions: " << stats.totalExecutions << "\n";
            oss << "  Success Rate: " << std::fixed << std::setprecision(2)
                << (stats.successRate * 100) << "%\n";
            oss << "  Avg Duration: " << std::fixed << std::setprecision(2)
                << stats.averageDuration << "ms\n";
            oss << "  Total Messages: " << stats.totalMessages << "\n\n";
        }

        oss << "---------- Overall ----------\n";
        oss << "Total Executions: " << totalExecutions << "\n";
        oss << "Total Successes: " << totalSuccesses << "\n";
        oss << "Overall Success Rate: " << std::fixed << std::setprecision(2)
            << (totalExecutions > 0 ? (static_cast<double>(totalSuccesses) / totalExecutions * 100) : 0.0)
            << "%\n";
        oss << "Total Duration: " << totalDuration << "ms\n";

        return oss.str();
    }

    // 生成详细报告
    std::string generateDetailedReport() const {
        std::ostringstream oss;
        oss << "========== Workflow Metrics Detailed Report ==========\n\n";

        if (metricsHistory_.empty()) {
            oss << "No metrics data available.\n";
            return oss.str();
        }

        for (const auto& pair : metricsHistory_) {
            oss << "Workflow: " << pair.first << "\n";
            oss << "----------------------------------------\n";

            for (const auto& metrics : pair.second) {
                oss << "  Execution #" << &metrics - &pair.second[0] + 1 << ":\n";
                oss << "    Start Time: " << metrics.startTime << "\n";
                oss << "    End Time: " << metrics.endTime << "\n";
                oss << "    Duration: " << metrics.duration << "ms\n";
                oss << "    State: " << metrics.startState << " -> " << metrics.endState << "\n";
                oss << "    Success: " << (metrics.success ? "Yes" : "No") << "\n";
                oss << "    Messages: " << metrics.messageCount << "\n";
                if (!metrics.errorMessage.empty()) {
                    oss << "    Error: " << metrics.errorMessage << "\n";
                }
                oss << "    Interrupts: " << metrics.interruptCount << "\n";
                oss << "    Pauses: " << metrics.pauseCount << "\n";
                oss << "    Resumes: " << metrics.resumeCount << "\n\n";
            }

            oss << "\n";
        }

        return oss.str();
    }

    // 生成 JSON 报告
    std::string generateJsonReport() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"workflows\": {\n";

        bool firstWorkflow = true;
        for (const auto& pair : metricsHistory_) {
            if (!firstWorkflow) {
                oss << ",\n";
            }
            firstWorkflow = false;

            oss << "    \"" << pair.first << "\": {\n";
            oss << "      \"executions\": [\n";

            bool firstExecution = true;
            for (const auto& metrics : pair.second) {
                if (!firstExecution) {
                    oss << ",\n";
                }
                firstExecution = false;

                oss << "        {\n";
                oss << "          \"startTime\": " << metrics.startTime << ",\n";
                oss << "          \"endTime\": " << metrics.endTime << ",\n";
                oss << "          \"duration\": " << metrics.duration << ",\n";
                oss << "          \"startState\": \"" << metrics.startState << "\",\n";
                oss << "          \"endState\": \"" << metrics.endState << "\",\n";
                oss << "          \"success\": " << (metrics.success ? "true" : "false") << ",\n";
                oss << "          \"errorMessage\": \"" << metrics.errorMessage << "\",\n";
                oss << "          \"messageCount\": " << metrics.messageCount << ",\n";
                oss << "          \"interruptCount\": " << metrics.interruptCount << ",\n";
                oss << "          \"pauseCount\": " << metrics.pauseCount << ",\n";
                oss << "          \"resumeCount\": " << metrics.resumeCount << "\n";
                oss << "        }";
            }

            oss << "\n      ]\n";
            oss << "    }";
        }

        oss << "\n  }\n";
        oss << "}\n";

        return oss.str();
    }

    // 生成 CSV 报告
    std::string generateCsvReport() const {
        std::ostringstream oss;
        oss << "Workflow,StartTime,EndTime,Duration,StartState,EndState,Success,ErrorMessage,MessageCount,InterruptCount,PauseCount,ResumeCount\n";

        for (const auto& pair : metricsHistory_) {
            for (const auto& metrics : pair.second) {
                oss << pair.first << ","
                    << metrics.startTime << ","
                    << metrics.endTime << ","
                    << metrics.duration << ","
                    << metrics.startState << ","
                    << metrics.endState << ","
                    << (metrics.success ? "true" : "false") << ","
                    << "\"" << metrics.errorMessage << "\","
                    << metrics.messageCount << ","
                    << metrics.interruptCount << ","
                    << metrics.pauseCount << ","
                    << metrics.resumeCount << "\n";
            }
        }

        return oss.str();
    }

    // 生成特定工作流的摘要报告
    std::string generateWorkflowSummaryReport(const std::string& workflowName,
                                             const std::vector<WorkflowMetrics>& metrics) const {
        std::ostringstream oss;
        oss << "========== Workflow Summary: " << workflowName << " ==========\n\n";

        if (metrics.empty()) {
            oss << "No metrics data available.\n";
            return oss.str();
        }

        int successCount = 0;
        int64_t totalDuration = 0;
        int64_t minDuration = INT64_MAX;
        int64_t maxDuration = 0;

        for (const auto& m : metrics) {
            if (m.success) {
                successCount++;
            }
            totalDuration += m.duration;
            minDuration = std::min(minDuration, m.duration);
            maxDuration = std::max(maxDuration, m.duration);
        }

        oss << "Total Executions: " << metrics.size() << "\n";
        oss << "Success Count: " << successCount << "\n";
        oss << "Success Rate: " << std::fixed << std::setprecision(2)
            << (static_cast<double>(successCount) / metrics.size() * 100) << "%\n";
        oss << "Total Duration: " << totalDuration << "ms\n";
        oss << "Average Duration: " << std::fixed << std::setprecision(2)
            << (static_cast<double>(totalDuration) / metrics.size()) << "ms\n";
        oss << "Min Duration: " << minDuration << "ms\n";
        oss << "Max Duration: " << maxDuration << "ms\n";

        return oss.str();
    }

    // 生成特定工作流的详细报告
    std::string generateWorkflowDetailedReport(const std::string& workflowName,
                                                const std::vector<WorkflowMetrics>& metrics) const {
        std::ostringstream oss;
        oss << "========== Workflow Detailed Report: " << workflowName << " ==========\n\n";

        if (metrics.empty()) {
            oss << "No metrics data available.\n";
            return oss.str();
        }

        for (size_t i = 0; i < metrics.size(); ++i) {
            const auto& m = metrics[i];
            oss << "Execution #" << (i + 1) << ":\n";
            oss << "  Start Time: " << m.startTime << "\n";
            oss << "  End Time: " << m.endTime << "\n";
            oss << "  Duration: " << m.duration << "ms\n";
            oss << "  State: " << m.startState << " -> " << m.endState << "\n";
            oss << "  Success: " << (m.success ? "Yes" : "No") << "\n";
            oss << "  Messages: " << m.messageCount << "\n";
            if (!m.errorMessage.empty()) {
                oss << "  Error: " << m.errorMessage << "\n";
            }
            oss << "  Interrupts: " << m.interruptCount << "\n";
            oss << "  Pauses: " << m.pauseCount << "\n";
            oss << "  Resumes: " << m.resumeCount << "\n\n";
        }

        return oss.str();
    }

    // 生成特定工作流的 JSON 报告
    std::string generateWorkflowJsonReport(const std::string& workflowName,
                                          const std::vector<WorkflowMetrics>& metrics) const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"workflow\": \"" << workflowName << "\",\n";
        oss << "  \"executions\": [\n";

        for (size_t i = 0; i < metrics.size(); ++i) {
            const auto& m = metrics[i];
            oss << "    {\n";
            oss << "      \"startTime\": " << m.startTime << ",\n";
            oss << "      \"endTime\": " << m.endTime << ",\n";
            oss << "      \"duration\": " << m.duration << ",\n";
            oss << "      \"startState\": \"" << m.startState << "\",\n";
            oss << "      \"endState\": \"" << m.endState << "\",\n";
            oss << "      \"success\": " << (m.success ? "true" : "false") << ",\n";
            oss << "      \"errorMessage\": \"" << m.errorMessage << "\",\n";
            oss << "      \"messageCount\": " << m.messageCount << ",\n";
            oss << "      \"interruptCount\": " << m.interruptCount << ",\n";
            oss << "      \"pauseCount\": " << m.pauseCount << ",\n";
            oss << "      \"resumeCount\": " << m.resumeCount << "\n";
            oss << "    }";
            if (i < metrics.size() - 1) {
                oss << ",";
            }
            oss << "\n";
        }

        oss << "  ]\n";
        oss << "}\n";

        return oss.str();
    }

    // 生成特定工作流的 CSV 报告
    std::string generateWorkflowCsvReport(const std::string& workflowName,
                                         const std::vector<WorkflowMetrics>& metrics) const {
        std::ostringstream oss;
        oss << "StartTime,EndTime,Duration,StartState,EndState,Success,ErrorMessage,MessageCount,InterruptCount,PauseCount,ResumeCount\n";

        for (const auto& m : metrics) {
            oss << m.startTime << ","
                << m.endTime << ","
                << m.duration << ","
                << m.startState << ","
                << m.endState << ","
                << (m.success ? "true" : "false") << ","
                << "\"" << m.errorMessage << "\","
                << m.messageCount << ","
                << m.interruptCount << ","
                << m.pauseCount << ","
                << m.resumeCount << "\n";
        }

        return oss.str();
    }

private:
    mutable std::recursive_mutex mutex_;
    std::map<std::string, std::vector<WorkflowMetrics>> metricsHistory_;
    std::map<std::string, WorkflowMetrics> activeMetrics_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_METRICS_COLLECTOR_IMPL_H
