/**
 * @file metrics_collector.cpp
 * @brief 业务流程管理系统 - 指标收集器实现
 */

#include "workflow_system/implementations/metrics_collector.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace WorkflowSystem {

MetricsCollector& MetricsCollector::getInstance() {
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::recordWorkflowStart(const std::string& workflowName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    WorkflowMetrics metrics;
    metrics.workflowName = workflowName;
    metrics.startTime = TimeUtils::getCurrentMs();
    metrics.startState = "RUNNING";
    metrics.success = false;
    metrics.messageCount = 0;
    metrics.interruptCount = 0;
    metrics.pauseCount = 0;
    metrics.resumeCount = 0;

    activeMetrics_[workflowName] = metrics;
    // 不在这里添加到历史记录，只有在结束时才添加

    LOG_INFO("[MetricsCollector] Started workflow: " + workflowName);
}

void MetricsCollector::recordWorkflowEnd(const std::string& workflowName, bool success,
                                        const std::string& errorMessage) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto it = activeMetrics_.find(workflowName);
    if (it == activeMetrics_.end()) {
        return;
    }

    it->second.endTime = TimeUtils::getCurrentMs();
    it->second.duration = it->second.endTime - it->second.startTime;
    it->second.success = success;
    it->second.errorMessage = errorMessage;
    it->second.endState = success ? "COMPLETED" : "FAILED";

    // 添加到历史记录
    metricsHistory_[workflowName].push_back(it->second);

    activeMetrics_.erase(it);

    LOG_INFO("[MetricsCollector] Ended workflow: " + workflowName +
             " success: " + std::string(success ? "true" : "false"));
}

void MetricsCollector::recordMessage(const std::string& workflowName,
                                    const std::string& messageType) {
    (void)messageType;
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto it = activeMetrics_.find(workflowName);
    if (it != activeMetrics_.end()) {
        it->second.messageCount++;
    }
}

void MetricsCollector::recordInterrupt(const std::string& workflowName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto it = activeMetrics_.find(workflowName);
    if (it != activeMetrics_.end()) {
        it->second.interruptCount++;
    }
}

void MetricsCollector::recordPause(const std::string& workflowName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto it = activeMetrics_.find(workflowName);
    if (it != activeMetrics_.end()) {
        it->second.pauseCount++;
    }
}

void MetricsCollector::recordResume(const std::string& workflowName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto it = activeMetrics_.find(workflowName);
    if (it != activeMetrics_.end()) {
        it->second.resumeCount++;
    }
}

std::vector<WorkflowMetrics> MetricsCollector::getWorkflowMetrics(
    const std::string& workflowName) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto it = metricsHistory_.find(workflowName);
    if (it != metricsHistory_.end()) {
        return it->second;
    }
    return std::vector<WorkflowMetrics>();
}

std::vector<WorkflowMetrics> MetricsCollector::getAllMetrics() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::vector<WorkflowMetrics> result;
    for (const auto& pair : metricsHistory_) {
        result.insert(result.end(), pair.second.begin(), pair.second.end());
    }
    return result;
}

WorkflowStatistics MetricsCollector::getStatistics(const std::string& workflowName) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    WorkflowStatistics stats;

    auto it = metricsHistory_.find(workflowName);
    if (it == metricsHistory_.end()) {
        return stats;
    }

    for (const auto& metrics : it->second) {
        stats.totalExecutions++;
        stats.totalDuration += metrics.duration;

        if (metrics.success) {
            stats.successCount++;
        } else {
            stats.failureCount++;
        }

        if (metrics.duration < stats.minDuration) {
            stats.minDuration = metrics.duration;
        }
        if (metrics.duration > stats.maxDuration) {
            stats.maxDuration = metrics.duration;
        }

        stats.totalMessages += metrics.messageCount;
    }

    if (stats.totalExecutions > 0) {
        stats.averageDuration = static_cast<double>(stats.totalDuration) / stats.totalExecutions;
        stats.successRate = static_cast<double>(stats.successCount) / stats.totalExecutions;
    }

    if (stats.minDuration == INT64_MAX) {
        stats.minDuration = 0;
    }

    return stats;
}

std::map<std::string, WorkflowStatistics> MetricsCollector::getAllStatistics() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::map<std::string, WorkflowStatistics> result;
    for (const auto& pair : metricsHistory_) {
        result[pair.first] = getStatistics(pair.first);
    }
    return result;
}

std::string MetricsCollector::generateReport(MetricsReportType type) const {
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

std::string MetricsCollector::generateWorkflowReport(const std::string& workflowName,
                                                    MetricsReportType type) const {
    auto metrics = getWorkflowMetrics(workflowName);

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

void MetricsCollector::clearMetrics(const std::string& workflowName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (workflowName.empty()) {
        metricsHistory_.clear();
        activeMetrics_.clear();
        LOG_INFO("[MetricsCollector] Cleared all metrics");
    } else {
        metricsHistory_.erase(workflowName);
        activeMetrics_.erase(workflowName);
        LOG_INFO("[MetricsCollector] Cleared metrics for: " + workflowName);
    }
}

bool MetricsCollector::exportMetrics(const std::string& filePath,
                                    MetricsReportType type) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::ofstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("[MetricsCollector] Failed to open file: " + filePath);
        return false;
    }

    std::string report = generateReport(type);
    file << report;
    file.close();

    LOG_INFO("[MetricsCollector] Exported metrics to: " + filePath);
    return true;
}

// ========== 私有方法实现 ==========

std::string MetricsCollector::generateSummaryReport() const {
    std::ostringstream oss;
    oss << "========== 工作流指标摘要报告 ==========\n\n";

    auto allStats = getAllStatistics();
    for (const auto& pair : allStats) {
        const auto& stats = pair.second;
        oss << "工作流: " << pair.first << "\n";
        oss << "  总执行次数: " << stats.totalExecutions << "\n";
        oss << "  成功次数: " << stats.successCount << "\n";
        oss << "  失败次数: " << stats.failureCount << "\n";
        oss << "  成功率: " << std::fixed << std::setprecision(2)
            << (stats.successRate * 100) << "%\n";
        oss << "  平均执行时长: " << std::fixed << std::setprecision(2)
            << stats.averageDuration << " ms\n";
        oss << "  最短执行时长: " << stats.minDuration << " ms\n";
        oss << "  最长执行时长: " << stats.maxDuration << " ms\n";
        oss << "  总消息数: " << stats.totalMessages << "\n\n";
    }

    oss << "======================================\n";
    return oss.str();
}

std::string MetricsCollector::generateDetailedReport() const {
    std::ostringstream oss;
    oss << "========== 工作流指标详细报告 ==========\n\n";

    for (const auto& pair : metricsHistory_) {
        oss << "工作流: " << pair.first << "\n";
        oss << "  执行记录数: " << pair.second.size() << "\n\n";

        int index = 1;
        for (const auto& metrics : pair.second) {
            oss << "  执行 #" << index++ << ":\n";
            oss << "    开始时间: " << metrics.startTime << "\n";
            oss << "    结束时间: " << metrics.endTime << "\n";
            oss << "    执行时长: " << metrics.duration << " ms\n";
            oss << "    状态: " << metrics.startState << " -> " << metrics.endState << "\n";
            oss << "    成功: " << (metrics.success ? "是" : "否") << "\n";
            if (!metrics.errorMessage.empty()) {
                oss << "    错误信息: " << metrics.errorMessage << "\n";
            }
            oss << "    消息数: " << metrics.messageCount << "\n";
            oss << "    中断次数: " << metrics.interruptCount << "\n";
            oss << "    暂停次数: " << metrics.pauseCount << "\n";
            oss << "    恢复次数: " << metrics.resumeCount << "\n\n";
        }
    }

    oss << "=======================================\n";
    return oss.str();
}

std::string MetricsCollector::generateJsonReport() const {
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

std::string MetricsCollector::generateCsvReport() const {
    std::ostringstream oss;
    oss << "WorkflowName,StartTime,EndTime,Duration,Success,ErrorMessage,";
    oss << "MessageCount,InterruptCount,PauseCount,ResumeCount\n";

    for (const auto& pair : metricsHistory_) {
        for (const auto& metrics : pair.second) {
            oss << pair.first << ",";
            oss << metrics.startTime << ",";
            oss << metrics.endTime << ",";
            oss << metrics.duration << ",";
            oss << (metrics.success ? "true" : "false") << ",";
            oss << "\"" << metrics.errorMessage << "\",";
            oss << metrics.messageCount << ",";
            oss << metrics.interruptCount << ",";
            oss << metrics.pauseCount << ",";
            oss << metrics.resumeCount << "\n";
        }
    }

    return oss.str();
}

std::string MetricsCollector::generateWorkflowSummaryReport(
    const std::string& workflowName,
    const std::vector<WorkflowMetrics>& metrics) const {
    (void)metrics;
    std::ostringstream oss;
    oss << "========== 工作流摘要报告: " << workflowName << " ==========\n\n";

    auto stats = getStatistics(workflowName);
    oss << "总执行次数: " << stats.totalExecutions << "\n";
    oss << "成功次数: " << stats.successCount << "\n";
    oss << "失败次数: " << stats.failureCount << "\n";
    oss << "成功率: " << std::fixed << std::setprecision(2)
        << (stats.successRate * 100) << "%\n";
    oss << "平均执行时长: " << std::fixed << std::setprecision(2)
        << stats.averageDuration << " ms\n";
    oss << "最短执行时长: " << stats.minDuration << " ms\n";
    oss << "最长执行时长: " << stats.maxDuration << " ms\n";
    oss << "总消息数: " << stats.totalMessages << "\n";

    oss << "\n======================================\n";
    return oss.str();
}

std::string MetricsCollector::generateWorkflowDetailedReport(
    const std::string& workflowName,
    const std::vector<WorkflowMetrics>& metrics) const {
    std::ostringstream oss;
    oss << "========== 工作流详细报告: " << workflowName << " ==========\n\n";
    oss << "执行记录数: " << metrics.size() << "\n\n";

    int index = 1;
    for (const auto& m : metrics) {
        oss << "执行 #" << index++ << ":\n";
        oss << "  开始时间: " << m.startTime << "\n";
        oss << "  结束时间: " << m.endTime << "\n";
        oss << "  执行时长: " << m.duration << " ms\n";
        oss << "  状态: " << m.startState << " -> " << m.endState << "\n";
        oss << "  成功: " << (m.success ? "是" : "否") << "\n";
        if (!m.errorMessage.empty()) {
            oss << "  错误信息: " << m.errorMessage << "\n";
        }
        oss << "  消息数: " << m.messageCount << "\n";
        oss << "  中断次数: " << m.interruptCount << "\n";
        oss << "  暂停次数: " << m.pauseCount << "\n";
        oss << "  恢复次数: " << m.resumeCount << "\n\n";
    }

    oss << "=======================================\n";
    return oss.str();
}

std::string MetricsCollector::generateWorkflowJsonReport(
    const std::string& workflowName,
    const std::vector<WorkflowMetrics>& metrics) const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"workflowName\": \"" << workflowName << "\",\n";
    oss << "  \"executions\": [\n";

    bool first = true;
    for (const auto& m : metrics) {
        if (!first) {
            oss << ",\n";
        }
        first = false;

        oss << "    {\n";
        oss << "      \"startTime\": " << m.startTime << ",\n";
        oss << "      \"endTime\": " << m.endTime << ",\n";
        oss << "      \"duration\": " << m.duration << ",\n";
        oss << "      \"success\": " << (m.success ? "true" : "false") << ",\n";
        oss << "      \"errorMessage\": \"" << m.errorMessage << "\",\n";
        oss << "      \"messageCount\": " << m.messageCount << ",\n";
        oss << "      \"interruptCount\": " << m.interruptCount << ",\n";
        oss << "      \"pauseCount\": " << m.pauseCount << ",\n";
        oss << "      \"resumeCount\": " << m.resumeCount << "\n";
        oss << "    }";
    }

    oss << "\n  ]\n";
    oss << "}\n";
    return oss.str();
}

std::string MetricsCollector::generateWorkflowCsvReport(
    const std::string& workflowName,
    const std::vector<WorkflowMetrics>& metrics) const {
    (void)workflowName;
    std::ostringstream oss;
    oss << "StartTime,EndTime,Duration,Success,ErrorMessage,";
    oss << "MessageCount,InterruptCount,PauseCount,ResumeCount\n";

    for (const auto& m : metrics) {
        oss << m.startTime << ",";
        oss << m.endTime << ",";
        oss << m.duration << ",";
        oss << (m.success ? "true" : "false") << ",";
        oss << "\"" << m.errorMessage << "\",";
        oss << m.messageCount << ",";
        oss << m.interruptCount << ",";
        oss << m.pauseCount << ",";
        oss << m.resumeCount << "\n";
    }

    return oss.str();
}

} // namespace WorkflowSystem
