/**
 * @file logger.h
 * @brief 日志模块兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_LOGGER_H
#define WORKFLOW_SYSTEM_LOGGER_H

#include "common/logger.h"

// 将 Common 命名空间的类引入 WorkflowSystem 命名空间
namespace WorkflowSystem {

// 导入 LogLevel
using Common::LogLevel;

// 导入 ILogger
using Common::ILogger;

// 导入 ConsoleLogger
using Common::ConsoleLogger;

// 导入 Logger
using Common::Logger;

} // namespace WorkflowSystem

// 日志宏保持兼容
#define LOG_INFO_WF(msg) Common::ConsoleLogger::getInstance().info(msg)
#define LOG_WARNING_WF(msg) Common::ConsoleLogger::getInstance().warning(msg)
#define LOG_ERROR_WF(msg) Common::ConsoleLogger::getInstance().error(msg)

#endif // WORKFLOW_SYSTEM_LOGGER_H
