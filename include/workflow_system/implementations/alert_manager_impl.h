/**
 * @file alert_manager_impl.h
 * @brief 告警管理器实现
 *
 * 功能：异常检测、告警触发、通知发送
 */

#ifndef WORKFLOW_SYSTEM_ALERT_MANAGER_IMPL_H
#define WORKFLOW_SYSTEM_ALERT_MANAGER_IMPL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <deque>
#include <algorithm>
#include "workflow_system/interfaces/alert_manager.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"

namespace WorkflowSystem {

/**
 * @brief 告警管理器实现
 */
class AlertManager : public IAlertManager {
public:
    AlertManager() : maxHistorySize_(1000) {}

    ~AlertManager() override = default;

    // ========== 告警规则管理 ==========

    std::string addAlertRule(const AlertConfig& config) override;
    bool removeAlertRule(const std::string& alertId) override;
    bool updateAlertRule(const std::string& alertId, const AlertConfig& config) override;
    bool enableAlertRule(const std::string& alertId, bool enabled) override;

    // ========== 告警触发 ==========

    void triggerAlert(AlertType type,
                     AlertLevel level,
                     const std::string& workflowName,
                     const std::string& message,
                     const std::string& details) override;
    void resolveAlert(const std::string& alertId) override;
    void acknowledgeAlert(const std::string& alertId) override;

    // ========== 查询接口 ==========

    std::vector<AlertRecord> getActiveAlerts() const override;
    std::vector<AlertRecord> getAlertHistory(const std::string& workflowName, int limit) const override;
    AlertConfig getAlertRule(const std::string& alertId) const override;
    std::vector<AlertConfig> getAllAlertRules() const override;

    // ========== 通知管理 ==========

    void setAlertCallback(AlertCallback callback) override;
    bool addNotificationChannel(const std::string& channel, const std::string& config) override;
    bool removeNotificationChannel(const std::string& channel) override;

    // ========== 统计接口 ==========

    int getAlertCount(const std::string& workflowName, int64_t timeWindowMs) const override;
    bool isThresholdExceeded(AlertType type, const std::string& workflowName) const override;

private:
    mutable std::mutex mutex_;
    int maxHistorySize_;

    std::unordered_map<std::string, AlertConfig> rules_;
    std::unordered_map<std::string, AlertRecord> activeAlerts_;
    std::deque<AlertRecord> history_;
    std::unordered_map<std::string, std::string> channels_;

    AlertCallback callback_;

    bool shouldTriggerAlert(const AlertConfig& rule, const std::string& workflowName);
    int getAlertCountInternal(const AlertConfig& rule) const;
    void createAndNotifyAlert(const AlertConfig& rule,
                             const std::string& workflowName,
                             const std::string& message,
                             const std::string& details);
    void trimHistory();
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ALERT_MANAGER_IMPL_H
