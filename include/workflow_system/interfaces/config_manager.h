/**
 * @file config_manager.h
 * @brief 配置管理器兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_CONFIG_MANAGER_H
#define WORKFLOW_SYSTEM_CONFIG_MANAGER_H

#include "common/config_manager.h"

namespace WorkflowSystem {

// 导入 ConfigFormat
using Common::ConfigFormat;

// 导入 ConfigValidationError
using Common::ConfigValidationError;

// 导入 ConfigChangeEvent
using Common::ConfigChangeEvent;

// 导入 IConfigManager
using Common::IConfigManager;

// 导入 ConfigRule
using Common::ConfigRule;

// 导入 IConfigValidator
using Common::IConfigValidator;

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CONFIG_MANAGER_H
