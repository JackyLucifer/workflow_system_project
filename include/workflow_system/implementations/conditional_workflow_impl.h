/**
 * @file conditional_workflow_impl.h
 * @brief 业务流程管理系统 - 条件分支工作流实现
 *
 * 设计模式：
 * - 策略模式：根据条件选择不同的执行路径
 * - 组合模式：组合多个工作流作为分支
 */

#ifndef WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_IMPL_H
#define WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_IMPL_H

#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <future>
#include <functional>
#include "workflow_system/interfaces/conditional_workflow.h"
#include "workflow_system/interfaces/workflow.h"
#include "workflow_system/interfaces/resource_manager.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/types.h"

namespace WorkflowSystem {

/**
 * @brief 条件评估器实现
 */
class ConditionEvaluator : public IConditionEvaluator {
public:
    bool evaluate(const std::string& conditionName,
                  std::shared_ptr<IWorkflowContext> context) override;

    void registerCondition(const std::string& name,
                           std::function<bool(std::shared_ptr<IWorkflowContext>)> condition) override;

    void removeCondition(const std::string& name) override;

private:
    std::map<std::string, std::function<bool(std::shared_ptr<IWorkflowContext>)>> conditions_;
};

/**
 * @brief 条件分支工作流实现
 *
 * 作为其他工作流的容器，支持条件分支选择
 */
class ConditionalWorkflow : public IConditionalWorkflow, public IWorkflow {
public:
    ConditionalWorkflow(std::shared_ptr<IResourceManager> resourceManager,
                        IConditionEvaluator* evaluator = nullptr);

    virtual ~ConditionalWorkflow();

    std::string getName() const override;

    WorkflowState getState() const override;

    void setContext(std::shared_ptr<IWorkflowContext> context) override;

    std::shared_ptr<IWorkflowContext> getContext() const override;

    // ========== 分支管理 ==========

    void addBranch(const Branch& branch) override;

    void setDefaultBranch(std::shared_ptr<IWorkflow> workflow) override;

    void setDefaultBranch(const Branch& branch) override;

    Branch* getSelectedBranch() override;

    std::vector<Branch> getAllBranches() const override;

    Branch* getDefaultBranch() override;

    void clearBranches() override;

    // ========== 执行逻辑 ==========

    bool evaluateAndExecute() override;

    // ========== IWorkflow 接口方法 ==========

    void start(std::shared_ptr<IWorkflowContext> context) override;

    void handleMessage(const IMessage& message) override;

    std::future<Any> execute() override;

    void interrupt() override;

    void cancel() override;

    void finish() override;

    void error(const std::string& errorMsg) override;

    // ========== 观察者管理方法 ==========

    void addObserver(std::shared_ptr<IWorkflowObserver> observer);
    void removeObserver(std::shared_ptr<IWorkflowObserver> observer);

    // ========== 事件处理方法 ==========

    void onWorkflowStarted(const std::string& workflowName);
    void onWorkflowFinished(const std::string& workflowName);
    void onWorkflowInterrupted(const std::string& workflowName);
    void onWorkflowError(const std::string& workflowName, const std::string& error);
    void setMessageHandler(MessageHandler handler);

private:
    /**
     * @brief 获取执行结果（阻塞等待）
     */
    Any getExecutionResult() const;

    /**
     * @brief 检查执行是否完成
     */
    bool isExecutionCompleted() const;

    /**
     * @brief 内部执行方法（在线程中运行）
     */
    void executeInternal(std::shared_ptr<std::promise<Any>> promise);

    std::string name_;
    std::shared_ptr<IResourceManager> resourceManager_;
    std::shared_ptr<IWorkflowContext> context_;
    WorkflowState state_;

    std::vector<Branch> branches_;
    Branch defaultBranch_;
    Branch* selectedBranch_;
    IConditionEvaluator* evaluator_;
    std::unique_ptr<ConditionEvaluator> defaultEvaluator_;

    std::thread executionThread_;
    std::atomic<bool> stopFlag_;
    std::atomic<bool> pausedFlag_;
    Any executionResult_;
    std::condition_variable pauseCV_;
    std::mutex threadMutex_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_IMPL_H
