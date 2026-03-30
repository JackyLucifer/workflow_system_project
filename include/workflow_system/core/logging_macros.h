/**
 * @file logging_macros.h
 * @brief 统一的日志宏（消除代码重复）
 *
 * 功能：
 * - 自动包含类名和方法名
 * - 简化日志调用
 * - 统一日志格式
 * - 消除50+处重复代码
 *
 * 使用示例：
 *   LOG_INFO_WF(WorkflowContext, "Context created");
 *   LOG_WARNING_WF(BaseWorkflow, "State transition failed");
 *   LOG_ERROR_WF(WorkflowExecutor, "Execution failed: " << errorMessage);
 */

#ifndef WORKFLOW_SYSTEM_LOGGING_MACROS_H
#define WORKFLOW_SYSTEM_LOGGING_MACROS_H

#include "logger.h"
#include <sstream>

namespace WorkflowSystem {
namespace Detail {

/**
 * @brief 日志格式化辅助类
 *
 * 使用流式接口构建日志消息
 */
class LogStream {
public:
    LogStream(const char* className, const char* methodName, const char* level)
        : className_(className), methodName_(methodName), level_(level) {}

    ~LogStream() {
        // 析构时自动记录日志
        std::string message = "[" + std::string(className_);
        if (methodName_) {
            message += "::";
            message += methodName_;
        }
        message += "] ";
        message += level_;
        message += ": ";
        message += stream_.str();

        // 根据级别记录日志
        if (std::string(level_) == "ERROR") {
            Logger::getInstance().error(message);
        } else if (std::string(level_) == "WARNING") {
            Logger::getInstance().warning(message);
        } else if (std::string(level_) == "INFO") {
            Logger::getInstance().info(message);
        } else {
            Logger::getInstance().info(message);  // DEBUG 使用 INFO 级别
        }
    }

    // 流式输出操作符
    template<typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    const char* className_;
    const char* methodName_;
    const char* level_;
    std::ostringstream stream_;
};

} // namespace Detail
} // namespace WorkflowSystem

// ========== 日志宏定义 ==========

// 基础宏（需要手动指定类名和方法名）
#define LOG_STREAM_WF(className, methodName, level) \
    WorkflowSystem::Detail::LogStream(#className, methodName, level)

// 便捷宏（自动使用类名和方法名）
#define LOG_INFO(className, methodName) \
    LOG_STREAM_WF(className, methodName, "INFO")

#define LOG_WARNING(className, methodName) \
    LOG_STREAM_WF(className, methodName, "WARNING")

#define LOG_ERROR(className, methodName) \
    LOG_STREAM_WF(className, methodName, "ERROR")

#define LOG_DEBUG(className, methodName) \
    LOG_STREAM_WF(className, methodName, "DEBUG")

// 简化宏（仅类名）
#define LOG_INFO_CLASS(className) \
    LOG_STREAM_WF(className, nullptr, "INFO")

#define LOG_WARNING_CLASS(className) \
    LOG_STREAM_WF(className, nullptr, "WARNING")

#define LOG_ERROR_CLASS(className) \
    LOG_STREAM_WF(className, nullptr, "ERROR")

// ========== 使用示例宏 ==========

/**
 * 使用示例：
 *
 * // 方式1：完整形式（推荐）
 * LOG_INFO(WorkflowContext, "get") << "Key not found: " << key;
 *
 * // 方式2：仅类名
 * LOG_ERROR_CLASS(WorkflowContext) << "Failed to set working directory";
 *
 * // 方式3：内联（临时调试）
 * if (error) {
 *     LOG_ERROR(MyWorkflow, "execute") << "Critical error: " << error;
 * }
 *
 * 输出格式：
 * [WorkflowContext::get] INFO: Key not found: myKey
 * [WorkflowContext] ERROR: Failed to set working directory
 */

// ========== 兼容性宏（向后兼容）==========

// 为了向后兼容，提供旧的宏接口
#define LOG_INFO_MSG(msg) Logger::getInstance().info(msg)
#define LOG_WARNING_MSG(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR_MSG(msg) Logger::getInstance().error(msg)
#define LOG_DEBUG_MSG(msg) Logger::getInstance().info(msg)  // DEBUG 使用 INFO 级别

// ========== 条件日志宏 ==========

// 调试专用（仅在调试模式启用）
#ifdef DEBUG
#define LOG_DEBUG_WF(className, method) LOG_DEBUG(className, method)
#else
#define LOG_DEBUG_WF(className, method) \
    if (false) LOG_STREAM_WF(className, method, "DEBUG")
#endif

// 性能敏感场景（只在满足条件时才格式化）
#define LOG_INFO_IF(condition, className, method) \
    if (condition) LOG_INFO(className, method)

#define LOG_ERROR_IF(condition, className, method) \
    if (condition) LOG_ERROR(className, method)

#define LOG_DEBUG_IF(condition, className, method) \
    if (condition) LOG_DEBUG(className, method)

#endif // WORKFLOW_SYSTEM_LOGGING_MACROS_H
