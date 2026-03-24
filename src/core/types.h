/**
 * @file types.h
 * @brief 业务流程管理系统 - 核心类型定义
 *
 * 设计模式：枚举类型用于状态和类型标识
 * 面向对象：使用强类型枚举确保类型安全
 */

#ifndef WORKFLOW_SYSTEM_TYPES_H
#define WORKFLOW_SYSTEM_TYPES_H

#include <string>

namespace WorkflowSystem {

/**
 * @brief 工作流状态枚举
 *
 * 设计模式：状态模式的前置条件
 */
enum class WorkflowState {
    IDLE,               // 空闲状态，工作流已注册但未启动
    INITIALIZING,       // 初始化中，准备启动工作流
    READY,              // 就绪状态，工作流已准备好执行
    PENDING,            // 等待中，已加入队列等待执行
    RUNNING,            // 运行中，正在执行工作流
    EXECUTING,          // 正在执行主体函数
    PAUSED,             // 已暂停，等待恢复
    SUSPENDED,          // 已挂起，等待唤醒
    INTERRUPTED,        // 已中断，强制停止
    TERMINATING,         // 正在终止，准备清理
    COMPLETED,          // 已完成，正常结束
    ERROR,               // 错误状态，执行出错
    FAILED,             // 执行失败（持久化使用）
    ABORTED,            // 已中止（持久化使用）
    CANCELED             // 已取消，用户取消操作
};

/**
 * @brief 资源类型枚举
 */
enum class ResourceType {
    DIRECTORY,      // 目录资源
    FILE,           // 文件资源
    TEMPORARY       // 临时资源
};

/**
 * @brief 将工作流状态转换为字符串
 */
inline std::string workflowStateToString(WorkflowState state) {
    switch (state) {
        case WorkflowState::IDLE: return "idle";
        case WorkflowState::INITIALIZING: return "initializing";
        case WorkflowState::READY: return "ready";
        case WorkflowState::PENDING: return "pending";
        case WorkflowState::RUNNING: return "running";
        case WorkflowState::EXECUTING: return "executing";
        case WorkflowState::PAUSED: return "paused";
        case WorkflowState::SUSPENDED: return "suspended";
        case WorkflowState::INTERRUPTED: return "interrupted";
        case WorkflowState::TERMINATING: return "terminating";
        case WorkflowState::COMPLETED: return "completed";
        case WorkflowState::ERROR: return "error";
        case WorkflowState::FAILED: return "failed";
        case WorkflowState::ABORTED: return "aborted";
        case WorkflowState::CANCELED: return "canceled";
        default: return "unknown";
    }
}

/**
 * @brief 将资源类型转换为字符串
 */
inline std::string resourceTypeToString(ResourceType type) {
    switch (type) {
        case ResourceType::DIRECTORY: return "directory";
        case ResourceType::FILE: return "file";
        case ResourceType::TEMPORARY: return "temporary";
        default: return "unknown";
    }
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_TYPES_H
