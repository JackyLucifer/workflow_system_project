/**
 * @file alert_manager_impl.cpp
 * @brief 告警管理器实现
 */

#include "workflow_system/implementations/alert_manager_impl.h"
#include "workflow_system/core/utils.h"

namespace WorkflowSystem {

// ========== 告警规则管理 ==========

std::string AlertManager::addAlertRule(const AlertConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string alertId = config.alertId;
    if (alertId.empty()) {
        alertId = "alert_" + IdGenerator::generate();
    }

    AlertConfig newConfig = config;
    newConfig.alertId = alertId;
    rules_[alertId] = newConfig;

    LOG_INFO("[AlertManager] Added alert rule: " + alertId +
             " type: " + alertTypeToString(config.type));

    return alertId;
}

bool AlertManager::removeAlertRule(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rules_.find(alertId);
    if (it == rules_.end()) {
        return false;
    }

    rules_.erase(it);
    LOG_INFO("[AlertManager] Removed alert rule: " + alertId);
    return true;
}

bool AlertManager::updateAlertRule(const std::string& alertId, const AlertConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rules_.find(alertId);
    if (it == rules_.end()) {
        return false;
    }

    it->second = config;
    it->second.alertId = alertId;

    LOG_INFO("[AlertManager] Updated alert rule: " + alertId);
    return true;
}

bool AlertManager::enableAlertRule(const std::string& alertId, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rules_.find(alertId);
    if (it == rules_.end()) {
        return false;
    }

    it->second.enabled = enabled;
    LOG_INFO("[AlertManager] " + std::string(enabled ? "Enabled" : "Disabled") +
             " alert rule: " + alertId);
    return true;
}

// ========== 告警触发 ==========

void AlertManager::triggerAlert(AlertType type,
                               AlertLevel level,
                               const std::string& workflowName,
                               const std::string& message,
                               const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否有匹配的告警规则
    bool hasMatchingRule = false;
    for (const auto& pair : rules_) {
        const auto& rule = pair.second;
        if (rule.enabled && rule.type == type) {
            if (rule.workflowName.empty() || rule.workflowName == workflowName) {
                hasMatchingRule = true;

                // 检查阈值
                if (shouldTriggerAlert(rule, workflowName)) {
                    createAndNotifyAlert(rule, workflowName, message, details);
                }
            }
        }
    }

    // 如果没有匹配规则，使用默认规则创建告警
    if (!hasMatchingRule) {
        AlertRecord record;
        record.alertId = "alert_" + IdGenerator::generate();
        record.alertName = alertTypeToString(type);
        record.type = type;
        record.level = level;
        record.workflowName = workflowName;
        record.message = message;
        record.details = details;
        record.triggeredAt = TimeUtils::getCurrentMs();
        record.resolved = false;
        record.count = 1;

        activeAlerts_[record.alertId] = record;
        history_.push_back(record);

        trimHistory();

        LOG_WARNING("[AlertManager] Triggered alert: " + record.alertId +
                   " level: " + alertLevelToString(level) + " message: " + message);

        if (callback_) {
            callback_(record);
        }
    }
}

void AlertManager::resolveAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeAlerts_.find(alertId);
    if (it == activeAlerts_.end()) {
        return;
    }

    it->second.resolved = true;
    it->second.resolvedAt = TimeUtils::getCurrentMs();

    // 更新历史记录
    for (auto& record : history_) {
        if (record.alertId == alertId && !record.resolved) {
            record.resolved = true;
            record.resolvedAt = it->second.resolvedAt;
        }
    }

    LOG_INFO("[AlertManager] Resolved alert: " + alertId);

    if (callback_) {
        callback_(it->second);
    }
}

void AlertManager::acknowledgeAlert(const std::string& alertId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeAlerts_.find(alertId);
    if (it != activeAlerts_.end()) {
        LOG_INFO("[AlertManager] Acknowledged alert: " + alertId);
    }
}

// ========== 查询接口 ==========

std::vector<AlertRecord> AlertManager::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AlertRecord> result;
    for (const auto& pair : activeAlerts_) {
        if (!pair.second.resolved) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<AlertRecord> AlertManager::getAlertHistory(const std::string& workflowName, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AlertRecord> result;
    for (auto rit = history_.rbegin();
         rit != history_.rend() && static_cast<int>(result.size()) < limit;
         ++rit) {
        if (workflowName.empty() || rit->workflowName == workflowName) {
            result.push_back(*rit);
        }
    }
    return result;
}

AlertConfig AlertManager::getAlertRule(const std::string& alertId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rules_.find(alertId);
    if (it != rules_.end()) {
        return it->second;
    }
    return AlertConfig();
}

std::vector<AlertConfig> AlertManager::getAllAlertRules() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AlertConfig> result;
    for (const auto& pair : rules_) {
        result.push_back(pair.second);
    }
    return result;
}

// ========== 通知管理 ==========

void AlertManager::setAlertCallback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

bool AlertManager::addNotificationChannel(const std::string& channel, const std::string& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_[channel] = config;
    LOG_INFO("[AlertManager] Added notification channel: " + channel);
    return true;
}

bool AlertManager::removeNotificationChannel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel);
    if (it != channels_.end()) {
        channels_.erase(it);
        LOG_INFO("[AlertManager] Removed notification channel: " + channel);
        return true;
    }
    return false;
}

// ========== 统计接口 ==========

int AlertManager::getAlertCount(const std::string& workflowName, int64_t timeWindowMs) const {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    int64_t cutoffTime = TimeUtils::getCurrentMs() - timeWindowMs;

    for (const auto& record : history_) {
        if (record.triggeredAt >= cutoffTime) {
            if (workflowName.empty() || record.workflowName == workflowName) {
                count++;
            }
        }
    }

    return count;
}

bool AlertManager::isThresholdExceeded(AlertType type, const std::string& workflowName) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& pair : rules_) {
        const auto& rule = pair.second;
        if (rule.type == type && rule.enabled) {
            if (rule.workflowName.empty() || rule.workflowName == workflowName) {
                int count = getAlertCountInternal(rule);
                if (count >= rule.threshold) {
                    return true;
                }
            }
        }
    }

    return false;
}

// ========== 私有方法实现 ==========

bool AlertManager::shouldTriggerAlert(const AlertConfig& rule, const std::string& workflowName) {
    int count = getAlertCountInternal(rule);
    return count >= rule.threshold;
}

int AlertManager::getAlertCountInternal(const AlertConfig& rule) const {
    int64_t cutoffTime = TimeUtils::getCurrentMs() - rule.timeWindowMs;
    int count = 0;

    for (const auto& record : history_) {
        if (record.triggeredAt >= cutoffTime &&
            record.type == rule.type &&
            (rule.workflowName.empty() || record.workflowName == rule.workflowName)) {
            count++;
        }
    }

    return count;
}

void AlertManager::createAndNotifyAlert(const AlertConfig& rule,
                                       const std::string& workflowName,
                                       const std::string& message,
                                       const std::string& details) {
    AlertRecord record;
    record.alertId = "alert_" + IdGenerator::generate();
    record.alertName = rule.alertId;
    record.type = rule.type;
    record.level = rule.level;
    record.workflowName = workflowName;
    record.message = message;
    record.details = details;
    record.triggeredAt = TimeUtils::getCurrentMs();
    record.resolved = false;
    record.count = getAlertCountInternal(rule) + 1;

    activeAlerts_[record.alertId] = record;
    history_.push_back(record);

    trimHistory();

    LOG_WARNING("[AlertManager] Triggered alert: " + record.alertId +
               " level: " + alertLevelToString(rule.level) +
               " message: " + message);

    if (callback_) {
        callback_(record);
    }
}

void AlertManager::trimHistory() {
    while (history_.size() > static_cast<size_t>(maxHistorySize_)) {
        history_.pop_front();
    }
}

} // namespace WorkflowSystem
