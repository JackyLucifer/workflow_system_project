/**
 * @file memory_pool.h
 * @brief 内存池兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_MEMORY_POOL_H
#define WORKFLOW_SYSTEM_MEMORY_POOL_H

#include "common/memory_pool.h"

namespace WorkflowSystem {

// 导入 MemoryPool
template<typename T, size_t PoolSize = 1024>
using MemoryPool = Common::MemoryPool<T, PoolSize>;

// 导入 ObjectPool
template<typename T>
using ObjectPool = Common::ObjectPool<T>;

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_MEMORY_POOL_H
