/**
 * @file timeout_handler.h
 * @brief 业务流程管理系统 - 超时处理器接口
 *
 * 设计模式：
 * - 策略模式：不同的超时处理策略
 *
 * 面向对象：
 * - 封装：封装超时处理逻辑
 */

#ifndef WORKFLOW_SYSTEM_TIMEOUT_HANDLER_H
#define WORKFLOW_SYSTEM_TIMEOUT_HANDLER_H

#include <string>
#include <functional>
#include <memory>
#include <chrono>

namespace WorkflowSystem {

/**
 * @brief 超时策略枚举
 */
enum class TimeoutPolicy {
    INTERRUPT,      // 中断工作流
    PAUSE,          // 暂停工作流
    RETRY,          // 重试工作流
    NOTIFY_ONLY     // 仅发送通知
};

/**
 * @brief 超时事件处理器
 */
using TimeoutCallback = std::function<void(const std::string& workflowName, int64_t elapsedMs)>;

/**
 * @brief 超时处理器接口
 */
class ITimeoutHandler {
public:
    virtual ~ITimeoutHandler() = default;

    /**
     * @brief 设置超时时间（毫秒）
     */
    virtual void setTimeout(int64_t timeoutMs) = 0;

    /**
     * @brief 获取超时时间（毫秒）
     */
    virtual int64_t getTimeout() const = 0;

    /**
     * @brief 设置超时策略
     */
    virtual void setPolicy(TimeoutPolicy policy) = 0;

    /**
     * @brief 获取超时策略
     */
    virtual TimeoutPolicy getPolicy() const = 0;

    /**
     * @brief 设置超时回调
     */
    virtual void setTimeoutCallback(TimeoutCallback callback) = 0;

    /**
     * @brief 启动超时计时
     */
    virtual void startTimer() = 0;

    /**
     * @brief 停止超时计时
     */
    virtual void stopTimer() = 0;

    /**
     * @brief 重置超时计时
     */
    virtual void resetTimer() = 0;

    /**
     * @brief 检查是否超时
     */
    virtual bool isTimedOut() const = 0;

    /**
     * @brief 获取已运行时间（毫秒）
     */
    virtual int64_t getElapsedTime() const = 0;

    /**
     * @brief 获取剩余时间（毫秒）
     */
    virtual int64_t getRemainingTime() const = 0;
};

/**
 * @brief 超时配置
 */
struct TimeoutConfig {
    int64_t timeoutMs = 0;           // 超时时间（毫秒），0表示无超时
    TimeoutPolicy policy = TimeoutPolicy::INTERRUPT;  // 超时策略
    TimeoutCallback callback;       // 超时回调
    bool autoRestart = false;       // 超时后是否自动重启工作流
    int maxRetries = 3;             // 最大重试次数（仅当 autoRestart 为 true 时有效）
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_TIMEOUT_HANDLER_H
