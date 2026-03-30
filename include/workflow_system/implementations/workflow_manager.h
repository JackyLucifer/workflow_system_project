/**
 * @file workflow_manager.h
 * @brief 业务流程管理系统 - 工作流管理器实现
 *
 * 设计模式：
 * - 门面模式：为整个工作流系统提供统一接口
 * - 单例模式：确保管理器全局唯一
 *
 * 面向对象：
 * - 封装：管理工作流的生命周期和切换
 * - 组合：包含资源管理器
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_MANAGER_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_MANAGER_IMPL_H

#include <unordered_map>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <future>
#include <sstream>
#include <algorithm>
#include "workflow_system/interfaces/workflow_manager.h"
#include "workflow_system/implementations/filesystem_resource_manager.h"
#include "workflow_system/implementations/workflow_context.h"
#include "workflow_system/implementations/sqlite_workflow_persistence.h"
#include "workflow_system/core/types.h"
#include "workflow_system/core/utils.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/any.h"

namespace WorkflowSystem {

/**
 * @brief 工作流管理器实现
 *
 * 职责：管理工作流的注册、启动、切换、中断和队列调度
 */
class WorkflowManager : public IWorkflowManager {
public:
    explicit WorkflowManager();
    virtual ~WorkflowManager();

    // ========== 工作流注册和获取 ==========
    bool registerWorkflow(const std::string& name, std::shared_ptr<IWorkflow> workflow) override;
    std::shared_ptr<IWorkflow> getWorkflow(const std::string& name) override;
    bool unregisterWorkflow(const std::string& name) override;

    // ========== 工作流控制 ==========
    bool startWorkflow(const std::string& name, std::shared_ptr<IWorkflowContext> context = nullptr) override;
    bool switchWorkflow(const std::string& name, bool preserveContext) override;
    void interrupt() override;
    void cancel() override;

    // ========== 工作流队列管理 ==========
    void enqueueWorkflow(const WorkflowTask& task) override;
    void dequeueWorkflow(const std::string& taskId) override;
    std::vector<WorkflowTask> getQueue() const override;
    void processQueue() override;
    bool isQueueProcessing() const override;
    size_t getQueueSize() const override;

    // ========== 消息处理 - 转发消息给当前工作流 ==========
    bool handleMessage(const IMessage& message) override;

    // ========== 资源管理 ==========
    void cleanupAllResources() override;

    // ========== 状态查询 ==========
    std::shared_ptr<IWorkflow> getCurrentWorkflow() const override;
    std::string getStatus() const override;
    std::shared_ptr<IResourceManager> getResourceManager() const override;

    // ========== 执行结果管理 ==========
    WorkflowExecutionResult getLastExecutionResult() const override;
    WorkflowExecutionResult getExecutionResult(const std::string& taskId) const override;
    std::vector<WorkflowExecutionResult> getAllExecutionResults() const override;

    // ========== 超时控制 ==========
    void setDefaultTimeout(long timeoutMs) override;
    long getDefaultTimeout() const override;

    // ========== 持久化支持 ==========
    bool saveWorkflowExecution(const std::string& workflowName,
                                         WorkflowState finalState,
                                         bool success,
                                         const std::string& errorMessage,
                                         int64_t durationMs) override;
    bool saveWorkflowDefinition(const std::string& workflowId,
                                           const std::string& definitionJson) override;
    std::string getWorkflowDefinition(const std::string& workflowId) const override;
    std::vector<std::string> getWorkflowExecutions(const std::string& workflowName) const override;

    /**
     * @brief 启用持久化功能
     * @param databasePath 数据库文件路径
     * @return 是否启用成功
     */
    bool enablePersistence(const std::string& databasePath);

    /**
     * @brief 禁用持久化功能
     */
    void disablePersistence();

    /**
     * @brief 获取持久化状态
     * @return 是否已启用持久化
     */
    bool isPersistenceEnabled() const;

private:
    // 启动队列处理线程
    void startQueueProcessing();

    // 停止队列处理线程
    void stopQueueProcessing();

    // 队列处理循环
    void queueProcessingLoop();

    // 执行单个任务
    void executeTask(const WorkflowTask& task);

    // 记录执行结果
    void recordExecutionResult(const WorkflowTask& task, const Any& result,
                           WorkflowState state, long startTime, long endTime,
                           const std::string& errorMessage);

    std::shared_ptr<IResourceManager> resourceManager_;
    std::unordered_map<std::string, std::shared_ptr<IWorkflow>> workflows_;
    std::shared_ptr<IWorkflow> currentWorkflow_;
    std::vector<std::unordered_map<std::string, std::string>> history_;
    mutable std::mutex mutex_;

    // 持久化支持
    IWorkflowPersistence* persistence_;
    std::string currentExecutionId_;  // 当前执行的ID

    // 队列相关
    std::queue<WorkflowTask> taskQueue_;
    std::atomic<bool> queueProcessing_;
    std::thread processingThread_;
    std::atomic<bool> processingThreadRunning_;
    std::condition_variable queueCV_;

    // 执行结果相关
    std::vector<WorkflowExecutionResult> executionResults_;
    long defaultTimeout_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_MANAGER_IMPL_H
