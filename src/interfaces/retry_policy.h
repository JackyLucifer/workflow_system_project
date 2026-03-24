/**
 * @file retry_policy.h
 * @brief 业务流程管理系统 - 重试策略接口
 *
 * 设计模式：
 * - 策略模式：不同的重试策略
 *
 * 面向对象：
 * - 封装：封装重试逻辑
 */

#ifndef WORKFLOW_SYSTEM_RETRY_POLICY_H
#define WORKFLOW_SYSTEM_RETRY_POLICY_H

#include <string>
#include <functional>
#include <chrono>

namespace WorkflowSystem {

/**
 * @brief 重试策略枚举
 */
enum class RetryStrategy {
    FIXED_DELAY,        // 固定延迟重试
    EXPONENTIAL_BACKOFF,  // 指数退避重试
    LINEAR_BACKOFF,     // 线性退避重试
    IMMEDIATE           // 立即重试
};

/**
 * @brief 重试条件枚举
 */
enum class RetryCondition {
    ON_ERROR,           // 错误时重试
    ON_TIMEOUT,         // 超时时重试
    ON_EXCEPTION,       // 异常时重试
    ON_FAILURE,         // 失败时重试（包括错误和超时）
    CUSTOM             // 自定义条件
};

/**
 * @brief 重试条件判断函数
 */
using RetryConditionCallback = std::function<bool(const std::string& error, int64_t elapsedMs)>;

/**
 * @brief 重试配置
 */
struct RetryConfig {
    int maxRetries = 3;                    // 最大重试次数
    RetryStrategy strategy = RetryStrategy::EXPONENTIAL_BACKOFF;  // 重试策略
    int64_t initialDelayMs = 1000;         // 初始延迟（毫秒）
    int64_t maxDelayMs = 30000;            // 最大延迟（毫秒）
    double backoffMultiplier = 2.0;        // 退避乘数（用于指数退避）
    int64_t delayIncrementMs = 1000;      // 延迟增量（用于线性退避）

    RetryCondition condition = RetryCondition::ON_FAILURE;  // 重试条件
    RetryConditionCallback customCondition;  // 自定义条件判断函数

    std::function<void(int attempt, const std::string& error)> onRetryCallback;  // 重试回调
};

/**
 * @brief 重试策略接口
 */
class IRetryPolicy {
public:
    virtual ~IRetryPolicy() = default;

    /**
     * @brief 检查是否应该重试
     */
    virtual bool shouldRetry(int attempt, const std::string& error, int64_t elapsedMs) = 0;

    /**
     * @brief 获取下一次重试的延迟时间（毫秒）
     */
    virtual int64_t getNextDelay(int attempt) const = 0;

    /**
     * @brief 重置重试计数
     */
    virtual void reset() = 0;

    /**
     * @brief 获取当前重试次数
     */
    virtual int getCurrentAttempt() const = 0;

    /**
     * @brief 获取最大重试次数
     */
    virtual int getMaxRetries() const = 0;

    /**
     * @brief 检查是否已达到最大重试次数
     */
    virtual bool hasReachedMaxRetries() const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_RETRY_POLICY_H
