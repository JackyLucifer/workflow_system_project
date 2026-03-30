/**
 * @file types.h
 * @brief 核心类型兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_TYPES_H
#define WORKFLOW_SYSTEM_TYPES_H

#include "common/types.h"

namespace WorkflowSystem {

// 导入 State 类型（使用 Common::State，但保留 WorkflowState 别名）
using Common::State;

// 为了向后兼容，保留 WorkflowState 作为 State 的别名
using WorkflowState = State;

// 导入 ResourceType
using Common::ResourceType;

// 导入工具函数
using Common::stateToString;
using Common::resourceTypeToString;

// 向后兼容：保留 workflowStateToString 函数
inline std::string workflowStateToString(WorkflowState state) {
    return stateToString(state);
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_TYPES_H
