/**
 * @file utils.h
 * @brief 工具类兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_UTILS_H
#define WORKFLOW_SYSTEM_UTILS_H

#include "common/utils.h"

namespace WorkflowSystem {

// 导入 IdGenerator
using Common::IdGenerator;

// 导入 TimeUtils
using Common::TimeUtils;

// 导入 StringUtils
using Common::StringUtils;

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_UTILS_H
