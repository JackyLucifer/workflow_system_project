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
#include "interfaces/conditional_workflow.h"
#include "interfaces/workflow.h"
#include "interfaces/resource_manager.h"
#include "core/logger.h"
#include "core/types.h"

namespace WorkflowSystem {

/**
 * @brief 条件评估器实现
 */
class ConditionEvaluator : public IConditionEvaluator {
public:
    bool evaluate(const std::string& conditionName,
                     std::shared_ptr<IWorkflowContext> context) override {
        auto it = conditions_.find(conditionName);
        if (it != conditions_.end()) {
            try {
                return it->second(context);
            } catch (const std::exception& e) {
                LOG_ERROR("[ConditionEvaluator] Error evaluating condition '" +
                         conditionName + "': " + std::string(e.what()));
                return false;
            }
        }

        LOG_WARNING("[ConditionEvaluator] Condition '" + conditionName + "' not found");
        return false;
    }

    void registerCondition(const std::string& name,
                               std::function<bool(std::shared_ptr<IWorkflowContext>)> condition) override {
        conditions_[name] = condition;
        LOG_INFO("[ConditionEvaluator] Registered condition: " + name);
    }

    void removeCondition(const std::string& name) override {
        conditions_.erase(name);
        LOG_INFO("[ConditionEvaluator] Removed condition: " + name);
    }

private:
    std::map<std::string, std::function<bool(std::shared_ptr<IWorkflowContext>)>> conditions_;
};

/**
 * @brief 条件分支工作流实现
 *
 * 作为其他工作流的容器，支持条件分支选择
 */
class ConditionalWorkflow : public IConditionalWorkflow {
public:
    ConditionalWorkflow(std::shared_ptr<IResourceManager> resourceManager,
                    IConditionEvaluator* evaluator = nullptr)
        : resourceManager_(resourceManager),
          selectedBranch_(nullptr),
          evaluator_(evaluator) {

        if (!evaluator_) {
            defaultEvaluator_.reset(new ConditionEvaluator());
            evaluator_ = defaultEvaluator_.get();
        }

        state_ = WorkflowState::IDLE;
        LOG_INFO("[ConditionalWorkflow] Created");
    }

    virtual ~ConditionalWorkflow() {
        // 停止并等待执行线程结束
        stopFlag_ = true;
        pausedFlag_ = false;
        pauseCV_.notify_all();  // 唤醒所有等待的线程

        // 等待线程结束（带超时）
        if (executionThread_.joinable()) {
            // 使用 join 等待线程完成
            // 线程应该在 stopFlag_ 为 true 时快速退出
            executionThread_.join();
        }

        // 清空所有分支
        clearBranches();
    }

    std::string getName() const {
        return name_;
    }

    WorkflowState getState() const {
        return state_;
    }

    void setContext(std::shared_ptr<IWorkflowContext> context) {
        context_ = context;
    }

    std::shared_ptr<IWorkflowContext> getContext() const {
        return context_;
    }

    // ========== 分支管理 ==========

    void addBranch(const Branch& branch) override {
        branches_.push_back(branch);
        LOG_INFO("[" + name_ + "] Added branch: " + branch.name);

        // 如果有条件函数，注册到评估器
        if (branch.condition) {
            evaluator_->registerCondition(branch.conditionName,
                [branch](std::shared_ptr<IWorkflowContext> ctx) {
                    return branch.condition();
                });
        }
    }

    void setDefaultBranch(std::shared_ptr<IWorkflow> workflow) override {
        defaultBranch_.name = "default";
        defaultBranch_.workflow = workflow;
        defaultBranch_.priority = -1;
        defaultBranch_.description = "Default branch when no conditions match";
        LOG_INFO("[" + name_ + "] Set default branch");
    }

    void setDefaultBranch(const Branch& branch) override {
        defaultBranch_ = branch;
        LOG_INFO("[" + name_ + "] Set default branch: " + branch.name);
    }

    Branch* getSelectedBranch() override {
        return selectedBranch_;
    }

    std::vector<Branch> getAllBranches() const override {
        return branches_;
    }

    Branch* getDefaultBranch() override {
        return &defaultBranch_;
    }

    void clearBranches() override {
        branches_.clear();
        selectedBranch_ = nullptr;
        LOG_INFO("[" + name_ + "] Cleared all branches");
    }

    // ========== 执行逻辑 ==========

    bool evaluateAndExecute() override {
        if (state_ != WorkflowState::READY && state_ != WorkflowState::RUNNING) {
            LOG_WARNING("[" + name_ + "] Workflow not ready to evaluate");
            return false;
        }

        // 评估所有分支条件
        Branch* selected = nullptr;
        int maxPriority = -1;

        for (size_t i = 0; i < branches_.size(); ++i) {
            Branch& branch = branches_[i];
            if (branch.condition) {
                // 使用条件函数评估
                bool result = false;
                try {
                    result = branch.condition();
                } catch (const std::exception& e) {
                    LOG_ERROR("[" + name_ + "] Error evaluating branch '" +
                             branch.name + "': " + std::string(e.what()));
                    continue;
                }

                if (result && branch.priority > maxPriority) {
                    selected = &branch;
                    maxPriority = branch.priority;
                    LOG_INFO("[" + name_ + "] Branch '" + branch.name +
                             "' condition matched (priority: " + std::to_string(branch.priority) + ")");
                }
            }
        }

        // 如果没有分支满足条件，使用默认分支
        if (!selected && defaultBranch_.workflow) {
            selected = &defaultBranch_;
            LOG_INFO("[" + name_ + "] No conditions matched, using default branch");
        }

        // 保存选中的分支
        selectedBranch_ = selected;

        if (!selected || !selected->workflow) {
            LOG_ERROR("[" + name_ + "] No valid branch to execute");
            return false;
        }

        // 传递上下文给选中的分支工作流
        selected->workflow->setContext(context_);

        // 启动选中的工作流
        LOG_INFO("[" + name_ + "] Starting branch: " + selected->name);
        state_ = WorkflowState::RUNNING;
        selected->workflow->start(context_);

        return true;
    }

    // ========== IWorkflow 接口方法 ==========

    void start(std::shared_ptr<IWorkflowContext> context) {
        if (context) {
            context_ = context;
        }

        state_ = WorkflowState::INITIALIZING;
        LOG_INFO("[" + name_ + "] Initializing workflow...");

        state_ = WorkflowState::READY;
        LOG_INFO("[" + name_ + "] Workflow ready");
    }

    void handleMessage(const IMessage& message) {
        // 转发给选中的分支工作流
        if (selectedBranch_ && selectedBranch_->workflow &&
            (state_ == WorkflowState::RUNNING || state_ == WorkflowState::EXECUTING)) {
            selectedBranch_->workflow->handleMessage(message);
        }
    }

    std::future<Any> execute() {
        // 只有 READY、RUNNING 或 EXECUTING 状态下才能执行
        if (state_ != WorkflowState::READY &&
            state_ != WorkflowState::RUNNING &&
            state_ != WorkflowState::EXECUTING) {
            LOG_WARNING("[" + name_ + "] Workflow not ready to execute (state: " +
                       workflowStateToString(state_) + "), cannot execute");
            // 返回一个已经设置错误结果的 future
            std::promise<Any> promise;
            promise.set_value(Any());
            return promise.get_future();
        }

        // 创建 promise 用于返回结果
        auto promise = std::make_shared<std::promise<Any>>();

        // 启动执行线程
        executionThread_ = std::thread(&ConditionalWorkflow::executeInternal, this, promise);

        return promise->get_future();
    }

    void interrupt() {
        if (selectedBranch_ && selectedBranch_->workflow) {
            WorkflowState state = selectedBranch_->workflow->getState();
            if (state == WorkflowState::RUNNING || state == WorkflowState::EXECUTING) {
                LOG_INFO("[" + name_ + "] Interrupting branch: " + selectedBranch_->name);
                selectedBranch_->workflow->interrupt();
            }
        }
        stopFlag_ = true;
    }

    void cancel() {
        stopFlag_ = true;
        LOG_INFO("[" + name_ + "] Cancelling workflow...");
        if (selectedBranch_ && selectedBranch_->workflow) {
            selectedBranch_->workflow->cancel();
        }
    }

    void finish() {
        stopFlag_ = true;
        LOG_INFO("[" + name_ + "] Finishing workflow...");
        if (selectedBranch_ && selectedBranch_->workflow) {
            selectedBranch_->workflow->finish();
        }
    }

    void error(const std::string& errorMsg) {
        LOG_ERROR("[" + name_ + "] Error: " + errorMsg);
        if (context_) {
            context_->set("last_error", errorMsg);
            if (selectedBranch_) {
                context_->set("error_branch", selectedBranch_->name);
            }
        }
    }

    // ========== 观察者接口方法（空实现）==========

    void addObserver(std::shared_ptr<IWorkflowObserver> observer) {}
    void removeObserver(std::shared_ptr<IWorkflowObserver> observer) {}
    void onWorkflowStarted(const std::string& workflowName) {}
    void onWorkflowFinished(const std::string& workflowName) {}
    void onWorkflowInterrupted(const std::string& workflowName) {}
    void onWorkflowError(const std::string& workflowName, const std::string& error) {}
    void setMessageHandler(MessageHandler handler) {}

private:
    /**
     * @brief 获取执行结果（阻塞等待）
     */
    Any getExecutionResult() const {
        if (executionResult_.has_value()) {
            return executionResult_;
        }
        return Any();
    }

    /**
     * @brief 检查执行是否完成
     */
    bool isExecutionCompleted() const {
        return executionResult_.has_value();
    }

    /**
     * @brief 内部执行方法（在线程中运行）
     */
    void executeInternal(std::shared_ptr<std::promise<Any>> promise) {
        try {
            state_ = WorkflowState::EXECUTING;
            LOG_INFO("[" + name_ + "] Starting execution in thread...");

            // 等待分支工作流完成
            if (selectedBranch_ && selectedBranch_->workflow) {
                auto future = selectedBranch_->workflow->execute();

                // 使用 wait_for 定期检查停止标志
                while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
                    if (stopFlag_) {
                        LOG_INFO("[" + name_ + "] Execution stopped by flag");
                        // 中断分支工作流
                        selectedBranch_->workflow->interrupt();
                        state_ = WorkflowState::INTERRUPTED;
                        promise->set_value(Any());
                        return;
                    }
                }

                // 获取分支工作流的结果
                std::string resultKey = "branch_result_" + selectedBranch_->name;
                std::string resultValue = context_->get(resultKey, "");
                LOG_INFO("[" + name_ + "] Branch '" + selectedBranch_->name +
                         "' completed with result: " + resultValue);

                // 设置结果到 promise
                promise->set_value(Any(resultValue));
            } else {
                promise->set_value(Any());
            }

            // 正常完成，自动转为 COMPLETED 状态
            state_ = WorkflowState::COMPLETED;
            LOG_INFO("[" + name_ + "] Execution completed");

        } catch (const std::exception& e) {
            error(std::string("Exception during execution: ") + e.what());
            promise->set_value(Any());
        } catch (...) {
            error("Unknown exception during execution");
            promise->set_value(Any());
        }
    }

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
    std::mutex threadMutex_;};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_IMPL_H
