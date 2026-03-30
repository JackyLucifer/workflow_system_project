/**
 * @file workflow_manager.h
 * @brief 业务流程管理系统 - 工作流管理器接口
 *
 * 设计模式：
 * - 门面模式：为复杂子系统提供简单接口
 * - 接口与实现分离
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_MANAGER_H
#define WORKFLOW_SYSTEM_WORKFLOW_MANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "workflow.h"
#include "resource_manager.h"
#include "workflow_system/core/any.h"

namespace WorkflowSystem {

/**
 * @brief 工作流队列任务
 *
 * 用于工作流队列中的任务描述
 */
struct WorkflowTask {
    std::string workflowName;
    std::shared_ptr<IWorkflowContext> context;
    std::function<void(const Any&)> onComplete; // 完成回调
    std::function<void(const std::string&)> onError; // 错误回调
    long timeoutMs; // 超时时间（毫秒），0表示无超时
    std::string taskId; // 任务ID
};

/**
 * @brief 工作流执行结果
 *
 * 存储工作流的执行结果信息
 */
struct WorkflowExecutionResult {
    std::string workflowName;
    Any result; // 执行结果
    WorkflowState finalState; // 最终状态
    long executionTimeMs; // 执行耗时（毫秒）
    std::string errorMessage; // 错误信息（如果有）
    long startTime; // 开始时间戳
    long endTime; // 结束时间戳
    std::string taskId; // 任务ID
};

/**
 * @brief 工作流管理器接口
 *
 * 设计模式：门面模式
 * 为整个工作流系统提供统一的访问接口
 */
class IWorkflowManager {
public:
    virtual ~IWorkflowManager() = default;

    // ========== 工作流注册和获取 ==========
    virtual bool registerWorkflow(const std::string& name, std::shared_ptr<IWorkflow> workflow) = 0;
    virtual std::shared_ptr<IWorkflow> getWorkflow(const std::string& name) = 0;
    virtual bool unregisterWorkflow(const std::string& name) = 0; // 新增：注销工作流

    // ========== 工作流控制 ==========
    virtual bool startWorkflow(const std::string& name, std::shared_ptr<IWorkflowContext> context = nullptr) = 0;
    virtual bool switchWorkflow(const std::string& name, bool preserveContext = true) = 0;
    virtual void interrupt() = 0;
    virtual void cancel() = 0; // 新增：取消当前任务

    // ========== 工作流队列管理 ==========
    virtual void enqueueWorkflow(const WorkflowTask& task) = 0; // 加入队列
    virtual void dequeueWorkflow(const std::string& taskId) = 0; // 从队列移除
    virtual std::vector<WorkflowTask> getQueue() const = 0; // 获取队列
    virtual void processQueue() = 0; // 处理队列
    virtual bool isQueueProcessing() const = 0; // 是否正在处理队列
    virtual size_t getQueueSize() const = 0; // 队列大小

    // ========== 消息处理 - 转发消息给当前工作流 ==========
    virtual bool handleMessage(const IMessage& message) = 0;

    // ========== 资源管理 ==========
    virtual void cleanupAllResources() = 0;

    // ========== 状态查询 ==========
    virtual std::shared_ptr<IWorkflow> getCurrentWorkflow() const = 0;
    virtual std::string getStatus() const = 0;
    virtual std::shared_ptr<IResourceManager> getResourceManager() const = 0;

    // ========== 执行结果管理 ==========
    virtual WorkflowExecutionResult getLastExecutionResult() const = 0; // 获取最后一次执行结果
    virtual WorkflowExecutionResult getExecutionResult(const std::string& taskId) const = 0; // 根据任务ID获取结果
    virtual std::vector<WorkflowExecutionResult> getAllExecutionResults() const = 0; // 获取所有执行历史

    // ========== 持久化支持 ==========
    virtual bool saveWorkflowExecution(const std::string& workflowName,
                                         WorkflowState finalState,
                                         bool success,
                                         const std::string& errorMessage,
                                         int64_t durationMs) = 0;
    virtual bool saveWorkflowDefinition(const std::string& workflowId,
                                           const std::string& definitionJson) = 0;
    virtual std::string getWorkflowDefinition(const std::string& workflowId) const = 0;
    virtual std::vector<std::string> getWorkflowExecutions(const std::string& workflowName) const = 0;

    // ========== 超时控制 ==========
    virtual void setDefaultTimeout(long timeoutMs) = 0; // 设置默认超时时间
    virtual long getDefaultTimeout() const = 0; // 获取默认超时时间
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_MANAGER_H
