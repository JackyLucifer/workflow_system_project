/**
 * @file logging_macros.h
 * @brief 日志宏兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_LOGGING_MACROS_H
#define WORKFLOW_SYSTEM_LOGGING_MACROS_H

#include "common/logging_macros.h"

// 日志宏直接使用 Common 命名空间的版本
#define LOG_STREAM_WF(className, methodName, level) \
    Common::Detail::LogStream(#className, methodName, level)

#define LOG_INFO_WF(className, methodName) \
    Common::Detail::LogStream(#className, methodName, "INFO")

#define LOG_WARNING_WF(className, methodName) \
    Common::Detail::LogStream(#className, methodName, "WARNING")

#define LOG_ERROR_WF(className, methodName) \
    Common::Detail::LogStream(#className, methodName, "ERROR")

#define LOG_DEBUG_WF(className, methodName) \
    Common::Detail::LogStream(#className, methodName, "DEBUG")

// 简化宏
#define LOG_INFO_CLASS_WF(className) \
    Common::Detail::LogStream(#className, nullptr, "INFO")

#define LOG_WARNING_CLASS_WF(className) \
    Common::Detail::LogStream(#className, nullptr, "WARNING")

#define LOG_ERROR_CLASS_WF(className) \
    Common::Detail::LogStream(#className, nullptr, "ERROR")

// 兼容性宏
#define LOG_INFO_MSG(msg) Common::Logger::getInstance().info(msg)
#define LOG_WARNING_MSG(msg) Common::Logger::getInstance().warning(msg)
#define LOG_ERROR_MSG(msg) Common::Logger::getInstance().error(msg)
#define LOG_DEBUG_MSG(msg) Common::Logger::getInstance().info(msg)

#endif // WORKFLOW_SYSTEM_LOGGING_MACROS_H
