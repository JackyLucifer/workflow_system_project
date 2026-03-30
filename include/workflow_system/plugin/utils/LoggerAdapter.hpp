#ifndef PLUGIN_FRAMEWORK_LOGGER_ADAPTER_HPP
#define PLUGIN_FRAMEWORK_LOGGER_ADAPTER_HPP

#include "workflow_system/core/logger.h"

namespace WorkflowSystem { namespace Plugin {

// 使用工作流系统的 Logger，添加类型别名以保持接口兼容
using Logger = WorkflowSystem::Logger;
using LogLevel = WorkflowSystem::LogLevel;

// 简化的日志宏，用于插件系统
#define PLUGIN_LOG_INFO(msg) WorkflowSystem::Logger::getInstance().info(msg)
#define PLUGIN_LOG_WARNING(msg) WorkflowSystem::Logger::getInstance().warning(msg)
#define PLUGIN_LOG_ERROR(msg) WorkflowSystem::Logger::getInstance().error(msg)

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_LOGGER_ADAPTER_HPP
