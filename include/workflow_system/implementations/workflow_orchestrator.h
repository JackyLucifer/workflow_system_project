/**
 * @file workflow_orchestrator.h
 * @brief 业务流程管理系统 - 工作流编排器实现
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_IMPL_H

#include "interfaces/workflow_orchestrator.h"
#include "interfaces/workflow_graph.h"
#include "interfaces/workflow_observer.h"
#include "core/logger.h"
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
    explicit OrchestrationContext(std::shared_ptr<IWorkflowGraph> graph)
        : graph_(graph), state_(OrchestrationState::IDLE) {}

    NodeExecutionResult getNodeResult(const std::string& nodeId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeResults_.find(nodeId);
        if (it != nodeResults_.end()) {
            return it->second;
        }
        NodeExecutionResult empty;
        empty.nodeId = nodeId;
        empty.success = false;
        empty.errorMessage = "No result available";
        empty.executionTimeMs = 0;
        return empty;
    }

    void setNodeResult(const std::string& nodeId,
                     const NodeExecutionResult& result) override {
        std::lock_guard<std::mutex> lock(mutex_);
        nodeResults_[nodeId] = result;
    }

    std::map<std::string, std::string> getAllVariables() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return variables_;
    }

    void setVariable(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        variables_[key] = value;
    }

    std::string getVariable(const std::string& key,
                          const std::string& defaultValue) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = variables_.find(key);
        return it != variables_.end() ? it->second : defaultValue;
    }

    std::shared_ptr<IWorkflowGraph> getGraph() const override { return graph_; }

    OrchestrationState getState() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    void setState(OrchestrationState state) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = state;
    }

    std::map<std::string, NodeExecutionResult> getAllResults() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nodeResults_;
    }

private:
    std::shared_ptr<IWorkflowGraph> graph_;
    OrchestrationState state_;
    std::map<std::string, NodeExecutionResult> nodeResults_;
    std::map<std::string, std::string> variables_;
    mutable std::mutex mutex_;
};

/**
 * @brief 默认节点执行器实现
 */
class DefaultNodeExecutor : public INodeExecutor {
public:
    NodeExecutionResult execute(
        const std::shared_ptr<IWorkflowNode>& node,
        IOrchestrationContext* /*context*/) override {

        auto startTime = std::chrono::steady_clock::now();

        NodeExecutionResult result;
        result.nodeId = node->getId();
        result.executionTimeMs = 0;

        LOG_INFO("[Orchestrator] Executing node: " + node->getName() +
                 " (" + node->getId() + ")");

        node->setState(NodeState::RUNNING);

        // 模拟执行时间
        auto timeout = node->getAttribute("timeout", "100");
        try {
            int timeoutMs = std::stoi(timeout);
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

            // 检查是否应该失败
            auto failMode = node->getAttribute("failMode", "false");
            if (failMode == "true") {
                throw std::runtime_error("Simulated failure");
            }

            result.success = true;
            result.errorMessage = "";

            // 添加一些输出数据
            result.output["status"] = "completed";
            result.output["node"] = node->getName();
            result.output["timestamp"] =
                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            node->setState(NodeState::COMPLETED);

        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = std::string("Execution failed: ") + e.what();
            node->setState(NodeState::FAILED);
            LOG_ERROR("[Orchestrator] Node failed: " + result.errorMessage);
        }

        auto endTime = std::chrono::steady_clock::now();
        result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        LOG_INFO("[Orchestrator] Node execution completed: " + node->getName() +
                 " - Success: " + (result.success ? "true" : "false") +
                 ", Time: " + std::to_string(result.executionTimeMs) + "ms");

        return result;
    }
};

/**
 * @brief 工作流编排器实现
 */
class WorkflowOrchestrator : public IWorkflowOrchestrator {
public:
    WorkflowOrchestrator()
        : state_(OrchestrationState::IDLE),
          strategy_(ExecutionStrategy::SEQUENTIAL),
          progress_(0),
          shouldAbort_(false),
          shouldPause_(false),
          workflowName_("Workflow") {}

    ~WorkflowOrchestrator() {
        abort();
    }

    void setGraph(std::shared_ptr<IWorkflowGraph> graph) override {
        std::lock_guard<std::mutex> lock(mutex_);
        graph_ = graph;
        if (graph) {
            setWorkflowName(graph->getName());
        }
        context_ = std::make_shared<OrchestrationContext>(graph);
    }

    std::shared_ptr<IWorkflowGraph> getGraph() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return graph_;
    }

    void setNodeExecutor(std::shared_ptr<INodeExecutor> executor) override {
        std::lock_guard<std::mutex> lock(mutex_);
        executor_ = executor;
    }

    bool execute() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 通知工作流开始
        notifyWorkflowStarted(workflowName_);

        if (!graph_ || !context_) {
            LOG_ERROR("[Orchestrator] No graph or context set");
            if (completionCallback_) {
                completionCallback_(false, "No graph or context set");
            }
            return false;
        }

        if (state_ == OrchestrationState::RUNNING) {
            LOG_WARNING("[Orchestrator] Already running");
            return false;
        }

        // 验证图
        if (!graph_->isDAG()) {
            auto cycles = graph_->detectCycles();
            LOG_ERROR("[Orchestrator] Graph is not a DAG. Found " +
                     std::to_string(cycles.size()) + " cycle(s)");
            if (completionCallback_) {
                completionCallback_(false, "Graph contains cycles");
            }
            return false;
        }

        state_ = OrchestrationState::RUNNING;
        context_->setState(OrchestrationState::RUNNING);
        shouldAbort_ = false;
        shouldPause_ = false;

        LOG_INFO("[Orchestrator] Starting workflow execution");

        bool success = false;
        std::string errorMessage;

        try {
            if (strategy_ == ExecutionStrategy::SEQUENTIAL) {
                success = executeSequential();
            } else if (strategy_ == ExecutionStrategy::PARALLEL) {
                success = executeParallel();
            } else {
                success = executeSequential(); // 默认顺序执行
            }

            if (success) {
                state_ = OrchestrationState::COMPLETED;
                context_->setState(OrchestrationState::COMPLETED);
                progress_ = 100;
                LOG_INFO("[Orchestrator] Workflow execution completed successfully");
                notifyWorkflowFinished(workflowName_);
            }
        } catch (const std::exception& e) {
            success = false;
            errorMessage = e.what();
            state_ = OrchestrationState::FAILED;
            context_->setState(OrchestrationState::FAILED);
            LOG_ERROR("[Orchestrator] Workflow execution failed: " + errorMessage);
            notifyWorkflowError(workflowName_, errorMessage);
        }

        if (progressCallback_) {
            progressCallback_(progress_);
        }

        if (completionCallback_) {
            completionCallback_(success, errorMessage);
        }

        return success;
    }

    void pause() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == OrchestrationState::RUNNING) {
            shouldPause_ = true;
            state_ = OrchestrationState::PAUSED;
            context_->setState(OrchestrationState::PAUSED);
            LOG_INFO("[Orchestrator] Workflow paused");
        }
    }

    void resume() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == OrchestrationState::PAUSED) {
            shouldPause_ = false;
            state_ = OrchestrationState::RUNNING;
            context_->setState(OrchestrationState::RUNNING);
            pauseCV_.notify_all();
            LOG_INFO("[Orchestrator] Workflow resumed");
        }
    }

    void abort() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == OrchestrationState::RUNNING ||
            state_ == OrchestrationState::PAUSED) {
            shouldAbort_ = true;
            shouldPause_ = false;
            pauseCV_.notify_all();
            LOG_INFO("[Orchestrator] Workflow aborted");
        }
    }

    OrchestrationState getState() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    IOrchestrationContext* getContext() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return context_.get();
    }

    std::vector<NodeExecutionResult> getAllResults() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!context_) {
            return std::vector<NodeExecutionResult>();
        }
        auto results = context_->getAllResults();
        std::vector<NodeExecutionResult> output;
        for (const auto& pair : results) {
            output.push_back(pair.second);
        }
        return output;
    }

    void setExecutionStrategy(ExecutionStrategy strategy) override {
        std::lock_guard<std::mutex> lock(mutex_);
        strategy_ = strategy;
    }

    ExecutionStrategy getExecutionStrategy() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return strategy_;
    }

    void setVariable(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (context_) {
            context_->setVariable(key, value);
        }
    }

    std::string getVariable(const std::string& key,
                          const std::string& defaultValue) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!context_) {
            return defaultValue;
        }
        return context_->getVariable(key, defaultValue);
    }

    int getProgress() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return progress_;
    }

    void setProgressCallback(ProgressCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        progressCallback_ = callback;
    }

    void setCompletionCallback(CompletionCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        completionCallback_ = callback;
    }

    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = OrchestrationState::IDLE;
        progress_ = 0;
        shouldAbort_ = false;
        shouldPause_ = false;
        if (context_) {
            context_->setState(OrchestrationState::IDLE);
        }
        LOG_INFO("[Orchestrator] Orchestrator reset");
    }

    // 观察者管理方法
    void addObserver(std::shared_ptr<IWorkflowObserver> observer) override {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }

    void removeObserver(std::shared_ptr<IWorkflowObserver> observer) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(observers_.begin(), observers_.end(),
            [&observer](const std::shared_ptr<IWorkflowObserver>& obs) {
                return obs.get() == observer.get();
            });
        if (it != observers_.end()) {
            observers_.erase(it);
        }
    }

    void clearObservers() override {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.clear();
    }

private:
    void setWorkflowName(const std::string& name) { workflowName_ = name; }

    /**
     * @brief 通知所有观察者工作流开始
     */
    void notifyWorkflowStarted(const std::string& workflowName) {
        for (auto& observer : observers_) {
            observer->onWorkflowStarted(workflowName);
        }
    }
    
    /**
     * @brief 通知所有观察者工作流完成
     */
    void notifyWorkflowFinished(const std::string& workflowName) {
        for (auto& observer : observers_) {
            observer->onWorkflowFinished(workflowName);
        }
    }
    
    /**
     * @brief 通知所有观察者工作流中断
     */
    void notifyWorkflowInterrupted(const std::string& workflowName) {
        for (auto& observer : observers_) {
            observer->onWorkflowInterrupted(workflowName);
        }
    }
    
    /**
     * @brief 通知所有观察者工作流错误
     */
    void notifyWorkflowError(const std::string& workflowName, const std::string& error) {
        for (auto& observer : observers_) {
            observer->onWorkflowError(workflowName, error);
        }
    }

    /**
     * @brief 顺序执行工作流
     */
    bool executeSequential() {
        if (!graph_ || !context_) {
            return false;
        }

        auto order = graph_->topologicalSort();
        if (order.empty()) {
            LOG_ERROR("[Orchestrator] Failed to get topological order");
            return false;
        }

        size_t totalNodes = order.size();
        for (size_t i = 0; i < order.size(); ++i) {
            // 检查终止
            if (shouldAbort_) {
                LOG_INFO("[Orchestrator] Execution aborted by user");
                notifyWorkflowInterrupted(workflowName_);
                return false;
            }

            // 检查暂停
            if (shouldPause_) {
                std::unique_lock<std::mutex> pauseLock(pauseMutex_);
                LOG_INFO("[Orchestrator] Paused, waiting to resume...");
                pauseCV_.wait(pauseLock, [this]() { return !shouldPause_ || shouldAbort_; });
                if (shouldAbort_) {
                    return false;
                }
            }

            const std::string& nodeId = order[i];
            auto node = graph_->getNode(nodeId);
            if (!node) {
                LOG_WARNING("[Orchestrator] Node not found: " + nodeId);
                continue;
            }

            // 更新进度
            progress_ = static_cast<int>((i * 100) / totalNodes);
            if (progressCallback_) {
                progressCallback_(progress_);
            }

            // 执行节点
            auto executor = executor_ ? executor_ :
                DefaultNodeExecutorFactory::create();
            NodeExecutionResult result = executor->execute(node, context_.get());

            // 保存结果
            context_->setNodeResult(nodeId, result);

            // 如果节点失败，根据配置决定是否继续
            if (!result.success) {
                auto failOnError = node->getAttribute("failOnError", "true");
                if (failOnError == "true") {
                    LOG_ERROR("[Orchestrator] Node failed, stopping execution: " + nodeId);
                    return false;
                } else {
                    LOG_WARNING("[Orchestrator] Node failed but continuing: " + nodeId);
                }
            }
        }

        progress_ = 100;
        return true;
    }

    /**
     * @brief 并行执行工作流
     */
    bool executeParallel() {
        if (!graph_ || !context_) {
            return false;
        }

        auto startNodes = graph_->getStartNodes();
        if (startNodes.empty()) {
            LOG_ERROR("[Orchestrator] No start nodes found");
            return false;
        }

        size_t totalNodes = graph_->getNodeCount();
        size_t completedNodes = 0;

        auto executor = executor_ ? executor_ :
            DefaultNodeExecutorFactory::create();

        // 使用简单的层级并行策略
        std::vector<std::string> currentLayer;
        // 跟踪所有已完成的节点（用于正确判断下一层）
        std::set<std::string> completedNodesSet;
        for (const auto& node : startNodes) {
            currentLayer.push_back(node->getId());
        }

        while (!currentLayer.empty() && !shouldAbort_) {
            std::vector<std::thread> threads;
            std::vector<std::string> nextLayer;
            std::vector<NodeExecutionResult> layerResults;

            // 执行当前层的所有节点
            for (const auto& nodeId : currentLayer) {
                if (shouldAbort_) {
                    break;
                }

                threads.emplace_back([&, executor, nodeId, this]() {
                    auto node = graph_->getNode(nodeId);
                    if (node) {
                        auto result = executor->execute(node, context_.get());
                        context_->setNodeResult(nodeId, result);
                        layerResults.push_back(result);
                    }
                });
            }

            // 等待所有线程完成
            for (auto& thread : threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }

            // 检查结果
            bool layerFailed = false;
            for (const auto& result : layerResults) {
                completedNodes++;
                if (!result.success) {
                    auto node = graph_->getNode(result.nodeId);
                    auto failOnError = node ? node->getAttribute("failOnError", "true") : "true";
                    if (failOnError == "true") {
                        layerFailed = true;
                    }
                }
            }

            if (layerFailed) {
                LOG_ERROR("[Orchestrator] Layer execution failed");
                return false;
            }

            // 更新进度
            progress_ = static_cast<int>((completedNodes * 100) / totalNodes);
            if (progressCallback_) {
                progressCallback_(progress_);
            }

            // 将当前层节点标记为已完成
            for (const auto& nodeId : currentLayer) {
                completedNodesSet.insert(nodeId);
            }

            // 找出下一层节点（所有前置节点都已执行完成）
            for (const auto& node : graph_->getAllNodes()) {
                const auto& nodeId = node->getId();
                // 跳过已经完成的节点
                if (completedNodesSet.find(nodeId) != completedNodesSet.end()) {
                    continue;
                }

                const auto& predecessors = node->getPredecessors();
                bool allPredsDone = true;
                for (const auto& predId : predecessors) {
                    // 检查前置节点是否已完成（不在 completedNodesSet 中说明未完成）
                    if (completedNodesSet.find(predId) == completedNodesSet.end()) {
                        allPredsDone = false;
                        break;
                    }
                }

                if (allPredsDone) {
                    nextLayer.push_back(nodeId);
                }
            }

            currentLayer = nextLayer;
        }

        progress_ = 100;
        return !shouldAbort_;
    }

private:
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
