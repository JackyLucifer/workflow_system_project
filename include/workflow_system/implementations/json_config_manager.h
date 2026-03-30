/**
 * @file json_config_manager.h
 * @brief JSON 配置管理器兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_JSON_CONFIG_MANAGER_H
#define WORKFLOW_SYSTEM_JSON_CONFIG_MANAGER_H

#include "common/json_config_manager.h"
#include "workflow_system/interfaces/config_manager.h"

namespace WorkflowSystem {

// 导入 SimpleJsonParser
using Common::SimpleJsonParser;

// 导入 JsonConfigManager（作为 Common::JsonConfigManager 的别名）
using JsonConfigManager = Common::JsonConfigManager;

// 导入 ConfigValidator
using ConfigValidator = Common::ConfigValidator;

// ConfigRules 是命名空间，需要特别处理
// 使用 inline namespace (C++11) 或者直接引用 Common::ConfigRules

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_JSON_CONFIG_MANAGER_H
