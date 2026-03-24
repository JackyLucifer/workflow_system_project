/**
 * @file alert_manager.h
 * @brief 告警管理器接口
 *
 * 功能：异常检测、告警触发、通知发送
 */

#ifndef WORKFLOW_SYSTEM_ALERT_MANAGER_H
#define WORKFLOW_SYSTEM_ALERT_MANAGER_H

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include "core/types.h"

namespace WorkflowSystem {

/**
 * @brief 告警级别
 */
enum class AlertLevel {
    INFO,           // 信息
    WARNING,        // 警告
    ERROR,          // 错误
    CRITICAL        // 严重
};

/**
 * @brief 告警类型
 */
enum class AlertType {
    WORKFLOW_FAILED,        // 工作流失败
    WORKFLOW_TIMEOUT,       // 工作流超时
    WORKFLOW_STUCK,         // 工作流卡住
    RESOURCE_EXHAUSTED,     // 资源耗尽
    HIGH_ERROR_RATE,        // 高错误率
    SCHEDULE_MISSED,        // 调度错过
    SYSTEM_OVERLOAD,        // 系统过载
    CUSTOM                  // 自定义
};

/**
 * @brief 告警配置
 */
struct AlertConfig {
    std::string alertId;            // 告警ID
    AlertType type;                 // 告警类型
    AlertLevel level;               // 告警级别
    std::string workflowName;       // 关联工作流（空表示全局）
    int threshold;                  // 阈值
    int timeWindowMs;               // 时间窗口（毫秒）
    bool enabled;                   // 是否启用
    bool notifyOnResolve;           // 恢复时是否通知
    std::vector<std::string> channels;  // 通知渠道
    
    AlertConfig()
        : type(AlertType::CUSTOM)
        , level(AlertLevel::WARNING)
        , threshold(1)
        , timeWindowMs(60000)
        , enabled(true)
        , notifyOnResolve(true) {}
};

/**
 * @brief 告警记录
 */
struct AlertRecord {
    std::string alertId;            // 告警ID
    std::string alertName;          // 告警名称
    AlertType type;                 // 告警类型
    AlertLevel level;               // 告警级别
    std::string workflowName;       // 工作流名称
    std::string message;            // 告警消息
    std::string details;            // 详细信息
    int64_t triggeredAt;            // 触发时间
    int64_t resolvedAt;             // 解决时间
    bool resolved;                  // 是否已解决
    int count;                      // 触发次数
    
    AlertRecord()
        : type(AlertType::CUSTOM)
        , level(AlertLevel::WARNING)
        , triggeredAt(0)
        , resolvedAt(0)
        , resolved(false)
        , count(0) {}
};

/**
 * @brief 告警回调类型
 */
using AlertCallback = std::function<void(const AlertRecord& alert)>;

/**
 * @brief 告警管理器接口
 */
class IAlertManager {
public:
    virtual ~IAlertManager() = default;
    
    // ========== 告警规则管理 ==========
    
    /**
     * @brief 添加告警规则
     * @param config 告警配置
     * @return 告警ID
     */
    virtual std::string addAlertRule(const AlertConfig& config) = 0;
    
    /**
     * @brief 移除告警规则
     * @param alertId 告警ID
     * @return 是否成功
     */
    virtual bool removeAlertRule(const std::string& alertId) = 0;
    
    /**
     * @brief 更新告警规则
     * @param alertId 告警ID
     * @param config 新配置
     * @return 是否成功
     */
    virtual bool updateAlertRule(const std::string& alertId, const AlertConfig& config) = 0;
    
    /**
     * @brief 启用/禁用告警规则
     * @param alertId 告警ID
     * @param enabled 是否启用
     * @return 是否成功
     */
    virtual bool enableAlertRule(const std::string& alertId, bool enabled) = 0;
    
    // ========== 告警触发 ==========
    
    /**
     * @brief 触发告警
     * @param type 告警类型
     * @param level 告警级别
     * @param workflowName 工作流名称
     * @param message 消息
     * @param details 详情
     */
    virtual void triggerAlert(AlertType type,
                             AlertLevel level,
                             const std::string& workflowName,
                             const std::string& message,
                             const std::string& details = "") = 0;
    
    /**
     * @brief 解决告警
     * @param alertId 告警ID
     */
    virtual void resolveAlert(const std::string& alertId) = 0;
    
    /**
     * @brief 确认告警
     * @param alertId 告警ID
     */
    virtual void acknowledgeAlert(const std::string& alertId) = 0;
    
    // ========== 查询接口 ==========
    
    /**
     * @brief 获取活跃告警
     * @return 告警列表
     */
    virtual std::vector<AlertRecord> getActiveAlerts() const = 0;
    
    /**
     * @brief 获取告警历史
     * @param workflowName 工作流名称（空表示全部）
     * @param limit 限制数量
     * @return 告警列表
     */
    virtual std::vector<AlertRecord> getAlertHistory(
        const std::string& workflowName = "", int limit = 100) const = 0;
    
    /**
     * @brief 获取告警规则
     * @param alertId 告警ID
     * @return 告警配置
     */
    virtual AlertConfig getAlertRule(const std::string& alertId) const = 0;
    
    /**
     * @brief 获取所有告警规则
     * @return 告警规则列表
     */
    virtual std::vector<AlertConfig> getAllAlertRules() const = 0;
    
    // ========== 通知管理 ==========
    
    /**
     * @brief 设置告警回调
     * @param callback 回调函数
     */
    virtual void setAlertCallback(AlertCallback callback) = 0;
    
    /**
     * @brief 添加通知渠道
     * @param channel 渠道名称
     * @param config 渠道配置（JSON格式）
     * @return 是否成功
     */
    virtual bool addNotificationChannel(const std::string& channel,
                                       const std::string& config) = 0;
    
    /**
     * @brief 移除通知渠道
     * @param channel 渠道名称
     * @return 是否成功
     */
    virtual bool removeNotificationChannel(const std::string& channel) = 0;
    
    // ========== 统计接口 ==========
    
    /**
     * @brief 获取告警统计
     * @param workflowName 工作流名称
     * @param timeWindowMs 时间窗口
     * @return 告警数量
     */
    virtual int getAlertCount(const std::string& workflowName = "",
                             int64_t timeWindowMs = 3600000) const = 0;
    
    /**
     * @brief 检查是否触发阈值
     * @param type 告警类型
     * @param workflowName 工作流名称
     * @return 是否超过阈值
     */
    virtual bool isThresholdExceeded(AlertType type,
                                    const std::string& workflowName = "") const = 0;
};

/**
 * @brief 告警级别转字符串
 */
inline std::string alertLevelToString(AlertLevel level) {
    switch (level) {
        case AlertLevel::INFO: return "INFO";
        case AlertLevel::WARNING: return "WARNING";
        case AlertLevel::ERROR: return "ERROR";
        case AlertLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 告警类型转字符串
 */
inline std::string alertTypeToString(AlertType type) {
    switch (type) {
        case AlertType::WORKFLOW_FAILED: return "workflow_failed";
        case AlertType::WORKFLOW_TIMEOUT: return "workflow_timeout";
        case AlertType::WORKFLOW_STUCK: return "workflow_stuck";
        case AlertType::RESOURCE_EXHAUSTED: return "resource_exhausted";
        case AlertType::HIGH_ERROR_RATE: return "high_error_rate";
        case AlertType::SCHEDULE_MISSED: return "schedule_missed";
        case AlertType::SYSTEM_OVERLOAD: return "system_overload";
        case AlertType::CUSTOM: return "custom";
        default: return "unknown";
    }
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ALERT_MANAGER_H
