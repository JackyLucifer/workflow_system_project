/**
 * @file alert_manager_simple.h
 * @brief 轻量级告警管理器（基础实现）
 *
 * 注意：这是一个简化的告警管理器实现，提供基础的告警触发和订阅功能。
 * 它不实现 IAlertManager 接口（该接口定义了完整的规则引擎、通知渠道等功能）。
 *
 * 适用场景：
 * - 小型项目，只需要基础的告警触发和记录
 * - 快速原型开发
 * - 不需要复杂告警规则和通知渠道的场景
 *
 * 如果需要完整的告警功能（规则引擎、Email/Webhook通知等），需要实现 IAlertManager 接口。
 *
 * 已实现功能：
 * - 告警触发
 * - 告警订阅（回调机制）
 * - 告警历史记录
 * - 线程安全
 *
 * 未实现功能（在 IAlertManager 中定义）：
 * - 告警规则管理
 * - 阈值检测
 * - 通知渠道（Email、Webhook等）
 * - 告警解决和确认
 * - 统计分析
 */

#ifndef WORKFLOW_SYSTEM_ALERT_MANAGER_SIMPLE_H
#define WORKFLOW_SYSTEM_ALERT_MANAGER_SIMPLE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <chrono>
#include <atomic>

namespace WorkflowSystem {

enum class AlertSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct Alert {
    std::string alertId;
    std::string ruleId;
    int severity;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

using AlertCallback = std::function<void(const Alert&)>;

class SimpleAlertManager {
public:
    static SimpleAlertManager& instance() {
        static SimpleAlertManager mgr;
        return mgr;
    }

    void trigger(const std::string& ruleId, int severity, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);

        Alert alert;
        alert.alertId = "alert_" + std::to_string(alertCounter_++);
        alert.ruleId = ruleId;
        alert.severity = severity;
        alert.message = message;
        alert.timestamp = std::chrono::system_clock::now();

        alerts_.push_back(alert);

        // 通知订阅者
        auto it = subscribers_.find(ruleId);
        if (it != subscribers_.end()) {
            for (auto& callback : it->second) {
                callback(alert);
            }
        }
    }

    void subscribe(const std::string& ruleId, AlertCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[ruleId].push_back(callback);
    }

    std::vector<Alert> getAlerts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return alerts_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        alerts_.clear();
    }

private:
    SimpleAlertManager() : alertCounter_(0) {}

    mutable std::mutex mutex_;
    std::vector<Alert> alerts_;
    std::unordered_map<std::string, std::vector<AlertCallback>> subscribers_;
    std::atomic<uint64_t> alertCounter_{0};
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ALERT_MANAGER_IMPL_H
