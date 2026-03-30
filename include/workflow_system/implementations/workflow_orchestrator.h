/**
 * @file workflow_orchestrator.h
 * @brief 业务流程管理系统 - 工作流编排器实现
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_IMPL_H

#include "workflow_system/interfaces/workflow_orchestrator.h"
#include "workflow_system/interfaces/workflow_graph.h"
#include "workflow_system/interfaces/workflow_observer.h"
#include "workflow_system/core/logger.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <set>

namespace WorkflowSystem {

/**
 * @brief 工作流执行上下文实现
 */
class OrchestrationContext : public IOrchestrationContext {
public:
    explicit OrchestrationContext(std::shared_ptr<IWorkflowGraph> graph);
    ~OrchestrationContext() override = default;

    NodeExecutionResult getNodeResult(const std::string& nodeId) const override;
    void setNodeResult(const std::string& nodeId,
                     const NodeExecutionResult& result) override;
    std::map<std::string, std::string> getAllVariables() const override;
    void setVariable(const std::string& key, const std::string& value) override;
    std::string getVariable(const std::string& key,
                          const std::string& defaultValue) const override;
    std::shared_ptr<IWorkflowGraph> getGraph() const override;
    OrchestrationState getState() const override;
    void setState(OrchestrationState state);
    std::map<std::string, NodeExecutionResult> getAllResults() const;

private:
    std::shared_ptr<IWorkflowGraph> graph_;
    OrchestrationState state_;
    std::map<std::string, NodeExecutionResult> nodeResults_;
    std::map<std::string, std::string> variables_;
    mutable std::mutex mutex_;
};

/**
 * @brief 默认节点执行器实现（带超时保护）
 */
class DefaultNodeExecutor : public INodeExecutor {
public:
    NodeExecutionResult execute(
        const std::shared_ptr<IWorkflowNode>& node,
        IOrchestrationContext* context) override;
};

/**
 * @brief 工作流编排器实现
 */
class WorkflowOrchestrator : public IWorkflowOrchestrator {
public:
    WorkflowOrchestrator();
    ~WorkflowOrchestrator() override;

    void setGraph(std::shared_ptr<IWorkflowGraph> graph) override;
    std::shared_ptr<IWorkflowGraph> getGraph() const override;
    void setNodeExecutor(std::shared_ptr<INodeExecutor> executor) override;
    bool execute() override;
    void pause() override;
    void resume() override;
    void abort() override;
    OrchestrationState getState() const override;
    IOrchestrationContext* getContext() const override;
    std::vector<NodeExecutionResult> getAllResults() const override;
    void setExecutionStrategy(ExecutionStrategy strategy) override;
    ExecutionStrategy getExecutionStrategy() const override;
    void setVariable(const std::string& key, const std::string& value) override;
    std::string getVariable(const std::string& key,
                          const std::string& defaultValue) const override;
    int getProgress() const override;
    void setProgressCallback(ProgressCallback callback) override;
    void setCompletionCallback(CompletionCallback callback) override;
    void reset() override;
    void addObserver(std::shared_ptr<IWorkflowObserver> observer) override;
    void removeObserver(std::shared_ptr<IWorkflowObserver> observer) override;
    void clearObservers() override;

private:
    void setWorkflowName(const std::string& name);
    void notifyWorkflowStarted(const std::string& workflowName);
    void notifyWorkflowFinished(const std::string& workflowName);
    void notifyWorkflowInterrupted(const std::string& workflowName);
    void notifyWorkflowError(const std::string& workflowName, const std::string& error);
    bool executeSequential();
    bool executeParallel();

    std::shared_ptr<IWorkflowGraph> graph_;
    std::shared_ptr<OrchestrationContext> context_;
    std::shared_ptr<INodeExecutor> executor_;
    OrchestrationState state_;
    ExecutionStrategy strategy_;
    int progress_;
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    std::atomic<bool> shouldAbort_;
    std::atomic<bool> shouldPause_;
    mutable std::mutex mutex_;
    std::mutex pauseMutex_;
    std::condition_variable pauseCV_;
    std::vector<std::shared_ptr<IWorkflowObserver>> observers_;
    std::string workflowName_;
};

/**
 * @brief 默认节点执行器工厂实现
 */
inline std::shared_ptr<INodeExecutor>
DefaultNodeExecutorFactory::create() {
    return std::make_shared<DefaultNodeExecutor>();
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_IMPL_H
