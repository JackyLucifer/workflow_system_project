/**
 * @file any.h
 * @brief Any 类型兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_ANY_H
#define WORKFLOW_SYSTEM_ANY_H

#include "common/any.h"

namespace WorkflowSystem {

// 导入 Any 类
using Common::Any;

// 导入 any_cast 函数
using Common::any_cast;

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ANY_H
