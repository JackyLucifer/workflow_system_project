/**
 * @file conditional_workflow_impl.cpp
 * @brief 业务流程管理系统 - 条件分支工作流实现
 */

#include "workflow_system/implementations/conditional_workflow_impl.h"

namespace WorkflowSystem {

// ========== ConditionEvaluator 实现 ==========

bool ConditionEvaluator::evaluate(const std::string& conditionName,
                                  std::shared_ptr<IWorkflowContext> context) {
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

void ConditionEvaluator::registerCondition(const std::string& name,
                                           std::function<bool(std::shared_ptr<IWorkflowContext>)> condition) {
    conditions_[name] = condition;
    LOG_INFO("[ConditionEvaluator] Registered condition: " + name);
}

void ConditionEvaluator::removeCondition(const std::string& name) {
    conditions_.erase(name);
    LOG_INFO("[ConditionEvaluator] Removed condition: " + name);
}

// ========== ConditionalWorkflow 实现 ==========

ConditionalWorkflow::ConditionalWorkflow(std::shared_ptr<IResourceManager> resourceManager,
                                         IConditionEvaluator* evaluator)
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

ConditionalWorkflow::~ConditionalWorkflow() {
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

std::string ConditionalWorkflow::getName() const {
    return name_;
}

WorkflowState ConditionalWorkflow::getState() const {
    return state_;
}

void ConditionalWorkflow::setContext(std::shared_ptr<IWorkflowContext> context) {
    context_ = context;
}

std::shared_ptr<IWorkflowContext> ConditionalWorkflow::getContext() const {
    return context_;
}

void ConditionalWorkflow::addBranch(const Branch& branch) {
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

void ConditionalWorkflow::setDefaultBranch(std::shared_ptr<IWorkflow> workflow) {
    defaultBranch_.name = "default";
    defaultBranch_.workflow = workflow;
    defaultBranch_.priority = -1;
    defaultBranch_.description = "Default branch when no conditions match";
    LOG_INFO("[" + name_ + "] Set default branch");
}

void ConditionalWorkflow::setDefaultBranch(const Branch& branch) {
    defaultBranch_ = branch;
    LOG_INFO("[" + name_ + "] Set default branch: " + branch.name);
}

Branch* ConditionalWorkflow::getSelectedBranch() {
    return selectedBranch_;
}

std::vector<Branch> ConditionalWorkflow::getAllBranches() const {
    return branches_;
}

Branch* ConditionalWorkflow::getDefaultBranch() {
    return &defaultBranch_;
}

void ConditionalWorkflow::clearBranches() {
    branches_.clear();
    selectedBranch_ = nullptr;
    LOG_INFO("[" + name_ + "] Cleared all branches");
}

bool ConditionalWorkflow::evaluateAndExecute() {
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

void ConditionalWorkflow::start(std::shared_ptr<IWorkflowContext> context) {
    if (context) {
        context_ = context;
    }

    state_ = WorkflowState::INITIALIZING;
    LOG_INFO("[" + name_ + "] Initializing workflow...");

    state_ = WorkflowState::READY;
    LOG_INFO("[" + name_ + "] Workflow ready");
}

void ConditionalWorkflow::handleMessage(const IMessage& message) {
    // 转发给选中的分支工作流
    if (selectedBranch_ && selectedBranch_->workflow &&
        (state_ == WorkflowState::RUNNING || state_ == WorkflowState::EXECUTING)) {
        selectedBranch_->workflow->handleMessage(message);
    }
}

std::future<Any> ConditionalWorkflow::execute() {
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

void ConditionalWorkflow::interrupt() {
    if (selectedBranch_ && selectedBranch_->workflow) {
        WorkflowState state = selectedBranch_->workflow->getState();
        if (state == WorkflowState::RUNNING || state == WorkflowState::EXECUTING) {
            LOG_INFO("[" + name_ + "] Interrupting branch: " + selectedBranch_->name);
            selectedBranch_->workflow->interrupt();
        }
    }
    stopFlag_ = true;
}

void ConditionalWorkflow::cancel() {
    stopFlag_ = true;
    LOG_INFO("[" + name_ + "] Cancelling workflow...");
    if (selectedBranch_ && selectedBranch_->workflow) {
        selectedBranch_->workflow->cancel();
    }
}

void ConditionalWorkflow::finish() {
    stopFlag_ = true;
    LOG_INFO("[" + name_ + "] Finishing workflow...");
    if (selectedBranch_ && selectedBranch_->workflow) {
        selectedBranch_->workflow->finish();
    }
}

void ConditionalWorkflow::error(const std::string& errorMsg) {
    LOG_ERROR("[" + name_ + "] Error: " + errorMsg);
    if (context_) {
        context_->set("last_error", errorMsg);
        if (selectedBranch_) {
            context_->set("error_branch", selectedBranch_->name);
        }
    }
}

void ConditionalWorkflow::addObserver(std::shared_ptr<IWorkflowObserver> observer) {
    (void)observer;
}

void ConditionalWorkflow::removeObserver(std::shared_ptr<IWorkflowObserver> observer) {
    (void)observer;
}

void ConditionalWorkflow::onWorkflowStarted(const std::string& workflowName) {
    (void)workflowName;
}

void ConditionalWorkflow::onWorkflowFinished(const std::string& workflowName) {
    (void)workflowName;
}

void ConditionalWorkflow::onWorkflowInterrupted(const std::string& workflowName) {
    (void)workflowName;
}

void ConditionalWorkflow::onWorkflowError(const std::string& workflowName, const std::string& error) {
    (void)workflowName;
    (void)error;
}

void ConditionalWorkflow::setMessageHandler(MessageHandler handler) {
    (void)handler;
}

Any ConditionalWorkflow::getExecutionResult() const {
    if (executionResult_.has_value()) {
        return executionResult_;
    }
    return Any();
}

bool ConditionalWorkflow::isExecutionCompleted() const {
    return executionResult_.has_value();
}

void ConditionalWorkflow::executeInternal(std::shared_ptr<std::promise<Any>> promise) {
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

} // namespace WorkflowSystem
