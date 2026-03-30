/**
 * @file async_logger.h
 * @brief 异步日志兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_ASYNC_LOGGER_H
#define WORKFLOW_SYSTEM_ASYNC_LOGGER_H

#include "common/async_logger.h"

namespace WorkflowSystem {

// 导入 LogEntry
using Common::LogEntry;

// 导入 AsyncLoggerConfig
using Common::AsyncLoggerConfig;

// 导入 LogFile
using Common::LogFile;

// 导入 AsyncLogger
using AsyncLogger = Common::AsyncLogger;

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ASYNC_LOGGER_H
