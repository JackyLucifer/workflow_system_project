/**
 * @file dead_letter_queue.h
 * @brief 死信队列接口 - 失败任务处理
 *
 * 功能：存储失败任务、重试处理、人工干预
 */

#ifndef WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_H
#define WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_H

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "workflow_system/core/types.h"
#include "workflow_system/core/any.h"

namespace WorkflowSystem {

/**
 * @brief 死信原因
 */
enum class DeadLetterReason {
    MAX_RETRIES_EXCEEDED,   // 超过最大重试次数
    TIMEOUT,                // 超时
    VALIDATION_ERROR,       // 验证错误
    EXECUTION_ERROR,        // 执行错误
    RESOURCE_UNAVAILABLE,   // 资源不可用
    DEPENDENCY_FAILED,      // 依赖失败
    CANCELLED,              // 已取消
    UNKNOWN                 // 未知原因
};

/**
 * @brief 死信状态
 */
enum class DeadLetterStatus {
    PENDING,        // 等待处理
    RETRYING,       // 重试中
    RESOLVED,       // 已解决
    DISCARDED,      // 已丢弃
    MANUAL_REVIEW   // 人工审核中
};

/**
 * @brief 死信项
 */
struct DeadLetterItem {
    std::string itemId;                 // 项ID
    std::string workflowName;           // 工作流名称
    std::string taskId;                 // 任务ID
    std::string executionId;            // 执行ID
    DeadLetterReason reason;            // 死信原因
    DeadLetterStatus status;            // 状态
    std::string errorMessage;           // 错误消息
    std::string stackTrace;             // 堆栈跟踪
    Any originalData;                   // 原始数据
    Any context;                        // 上下文
    int64_t createdAt;                  // 创建时间
    int64_t lastAttemptAt;              // 最后尝试时间
    int retryCount;                     // 重试次数
    int maxRetries;                     // 最大重试次数
    std::string resolution;             // 解决方案
    std::string resolvedBy;             // 解决者
    int64_t resolvedAt;                 // 解决时间
    
    DeadLetterItem()
        : reason(DeadLetterReason::UNKNOWN)
        , status(DeadLetterStatus::PENDING)
        , createdAt(0)
        , lastAttemptAt(0)
        , retryCount(0)
        , maxRetries(3)
        , resolvedAt(0) {}
};

/**
 * @brief 死信队列配置
 */
struct DeadLetterQueueConfig {
    int maxItems;                        // 最大项数
    int64_t retentionMs;                 // 保留时间（毫秒）
    int maxRetries;                      // 默认最大重试次数
    bool autoRetry;                      // 是否自动重试
    int64_t retryDelayMs;                // 重试延迟
    bool enableNotifications;            // 启用通知
    
    DeadLetterQueueConfig()
        : maxItems(10000)
        , retentionMs(7 * 24 * 60 * 60 * 1000)
        , maxRetries(3)
        , autoRetry(false)
        , retryDelayMs(60000)
        , enableNotifications(true) {}
};

/**
 * @brief 死信处理回调
 */
using DeadLetterCallback = std::function<void(const DeadLetterItem& item)>;

/**
 * @brief 死信队列接口
 */
class IDeadLetterQueue {
public:
    virtual ~IDeadLetterQueue() = default;
    
    // ========== 入队操作 ==========
    
    /**
     * @brief 添加死信项
     * @param item 死信项
     * @return 项ID
     */
    virtual std::string enqueue(const DeadLetterItem& item) = 0;
    
    /**
     * @brief 从工作流错误创建死信项
     * @param workflowName 工作流名称
     * @param errorMessage 错误消息
     * @param reason 死信原因
     * @return 项ID
     */
    virtual std::string enqueueFromError(const std::string& workflowName,
                                         const std::string& errorMessage,
                                         DeadLetterReason reason) = 0;
    
    // ========== 出队和查询 ==========
    
    /**
     * @brief 获取死信项
     * @param itemId 项ID
     * @return 死信项
     */
    virtual DeadLetterItem getItem(const std::string& itemId) const = 0;
    
    /**
     * @brief 获取所有待处理项
     * @return 死信项列表
     */
    virtual std::vector<DeadLetterItem> getPendingItems() const = 0;
    
    /**
     * @brief 获取指定状态的项
     * @param status 状态
     * @param limit 限制数量
     * @return 死信项列表
     */
    virtual std::vector<DeadLetterItem> getItemsByStatus(
        DeadLetterStatus status, int limit = 100) const = 0;
    
    /**
     * @brief 获取工作流的死信项
     * @param workflowName 工作流名称
     * @return 死信项列表
     */
    virtual std::vector<DeadLetterItem> getItemsByWorkflow(
        const std::string& workflowName) const = 0;
    
    /**
     * @brief 获取队列大小
     * @return 队列大小
     */
    virtual size_t size() const = 0;
    
    /**
     * @brief 检查队列是否为空
     * @return 是否为空
     */
    virtual bool isEmpty() const = 0;
    
    // ========== 重试操作 ==========
    
    /**
     * @brief 重试死信项
     * @param itemId 项ID
     * @return 是否成功
     */
    virtual bool retry(const std::string& itemId) = 0;
    
    /**
     * @brief 批量重试
     * @param itemIds 项ID列表
     * @return 成功数量
     */
    virtual int retryBatch(const std::vector<std::string>& itemIds) = 0;
    
    /**
     * @brief 重试所有待处理项
     * @return 成功数量
     */
    virtual int retryAll() = 0;
    
    // ========== 解决操作 ==========
    
    /**
     * @brief 标记为已解决
     * @param itemId 项ID
     * @param resolution 解决方案
     * @param resolvedBy 解决者
     * @return 是否成功
     */
    virtual bool resolve(const std::string& itemId,
                        const std::string& resolution,
                        const std::string& resolvedBy = "") = 0;
    
    /**
     * @brief 丢弃死信项
     * @param itemId 项ID
     * @param reason 丢弃原因
     * @return 是否成功
     */
    virtual bool discard(const std::string& itemId,
                        const std::string& reason = "") = 0;
    
    /**
     * @brief 标记为人工审核
     * @param itemId 项ID
     * @return 是否成功
     */
    virtual bool markForManualReview(const std::string& itemId) = 0;
    
    // ========== 管理操作 ==========
    
    /**
     * @brief 清除已解决的项
     * @return 清除数量
     */
    virtual int clearResolved() = 0;
    
    /**
     * @brief 清除所有项
     */
    virtual void clearAll() = 0;
    
    /**
     * @brief 清除过期项
     * @return 清除数量
     */
    virtual int clearExpired() = 0;
    
    /**
     * @brief 设置配置
     * @param config 配置
     */
    virtual void setConfig(const DeadLetterQueueConfig& config) = 0;
    
    /**
     * @brief 获取配置
     * @return 配置
     */
    virtual DeadLetterQueueConfig getConfig() const = 0;
    
    // ========== 回调设置 ==========
    
    /**
     * @brief 设置入队回调
     * @param callback 回调函数
     */
    virtual void setEnqueueCallback(DeadLetterCallback callback) = 0;
    
    /**
     * @brief 设置重试回调
     * @param callback 回调函数
     */
    virtual void setRetryCallback(DeadLetterCallback callback) = 0;
    
    /**
     * @brief 设置解决回调
     * @param callback 回调函数
     */
    virtual void setResolveCallback(DeadLetterCallback callback) = 0;
    
    // ========== 统计 ==========
    
    /**
     * @brief 获取统计信息
     * @return JSON格式的统计信息
     */
    virtual std::string getStatistics() const = 0;
    
    /**
     * @brief 按原因统计数量
     * @return 原因到数量的映射
     */
    virtual std::unordered_map<DeadLetterReason, int> countByReason() const = 0;
    
    /**
     * @brief 按状态统计数量
     * @return 状态到数量的映射
     */
    virtual std::unordered_map<DeadLetterStatus, int> countByStatus() const = 0;
};

/**
 * @brief 死信原因转字符串
 */
inline std::string deadLetterReasonToString(DeadLetterReason reason) {
    switch (reason) {
        case DeadLetterReason::MAX_RETRIES_EXCEEDED: return "max_retries_exceeded";
        case DeadLetterReason::TIMEOUT: return "timeout";
        case DeadLetterReason::VALIDATION_ERROR: return "validation_error";
        case DeadLetterReason::EXECUTION_ERROR: return "execution_error";
        case DeadLetterReason::RESOURCE_UNAVAILABLE: return "resource_unavailable";
        case DeadLetterReason::DEPENDENCY_FAILED: return "dependency_failed";
        case DeadLetterReason::CANCELLED: return "cancelled";
        case DeadLetterReason::UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

/**
 * @brief 死信状态转字符串
 */
inline std::string deadLetterStatusToString(DeadLetterStatus status) {
    switch (status) {
        case DeadLetterStatus::PENDING: return "pending";
        case DeadLetterStatus::RETRYING: return "retrying";
        case DeadLetterStatus::RESOLVED: return "resolved";
        case DeadLetterStatus::DISCARDED: return "discarded";
        case DeadLetterStatus::MANUAL_REVIEW: return "manual_review";
        default: return "unknown";
    }
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_H
