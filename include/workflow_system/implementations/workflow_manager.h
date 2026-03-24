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
#include "interfaces/workflow_manager.h"
#include "implementations/filesystem_resource_manager.h"
#include "implementations/workflow_context.h"
#include "implementations/sqlite_workflow_persistence.h"
#include "core/types.h"
#include "core/utils.h"
#include "core/logger.h"
#include "core/any.h"

namespace WorkflowSystem {

/**
 * @brief 工作流管理器实现
 *
 * 职责：管理工作流的注册、启动、切换、中断和队列调度
 */
class WorkflowManager : public IWorkflowManager {
public:
    explicit WorkflowManager()
        : resourceManager_(std::make_shared<FileSystemResourceManager>()),
          workflows_(),
          currentWorkflow_(),
          history_(),
          mutex_(),
          persistence_(nullptr),
          currentExecutionId_(),
          taskQueue_(),
          queueProcessing_(false),
          processingThread_(),
          processingThreadRunning_(false),
          queueCV_(),
          executionResults_(),
          defaultTimeout_(0) {}

    virtual ~WorkflowManager() {
        stopQueueProcessing();
    }

    // ========== 工作流注册和获取 ==========
    bool registerWorkflow(const std::string& name, std::shared_ptr<IWorkflow> workflow) override {
        std::lock_guard<std::mutex> lock(mutex_);
        workflows_[name] = workflow;
        LOG_INFO("[WorkflowManager] Registered workflow: " + name);
        return true;
    }

    std::shared_ptr<IWorkflow> getWorkflow(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = workflows_.find(name);
        return (it != workflows_.end()) ? it->second : nullptr;
    }

    bool unregisterWorkflow(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = workflows_.find(name);
        if (it != workflows_.end()) {
            // 如果是当前工作流，先中断
            if (currentWorkflow_ && currentWorkflow_->getName() == name) {
                currentWorkflow_->interrupt();
                currentWorkflow_ = nullptr;
            }
            workflows_.erase(it);
            LOG_INFO("[WorkflowManager] Unregistered workflow: " + name);
            return true;
        }
        return false;
    }

    // ========== 工作流控制 ==========
    bool startWorkflow(const std::string& name, std::shared_ptr<IWorkflowContext> context = nullptr) override {
        std::shared_ptr<IWorkflow> workflow;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = workflows_.find(name);
            if (it == workflows_.end()) {
                LOG_ERROR("[WorkflowManager] Workflow '" + name + "' not found");
                return false;
            }
            workflow = it->second;
        }

        // 中断当前工作流并清理其上下文
        if (currentWorkflow_) {
            currentWorkflow_->interrupt();
            auto oldContext = currentWorkflow_->getContext();
            if (oldContext) {
                oldContext->cleanup();
            }
        }

        // 使用传入的上下文，或创建新的上下文
        std::shared_ptr<IWorkflowContext> finalContext = context;
        if (!finalContext) {
            finalContext = std::make_shared<WorkflowContext>(resourceManager_);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentWorkflow_ = workflow;
        }
        currentWorkflow_->start(finalContext);

        return true;
    }

    bool switchWorkflow(const std::string& name, bool preserveContext) override {
        std::shared_ptr<IWorkflow> workflow;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = workflows_.find(name);
            if (it == workflows_.end()) {
                LOG_ERROR("[WorkflowManager] Workflow '" + name + "' not found");
                return false;
            }
            workflow = it->second;
        }

        auto previousWorkflow = getCurrentWorkflow();

        // 获取当前上下文
        std::shared_ptr<IWorkflowContext> context = nullptr;
        if (preserveContext && previousWorkflow) {
            context = previousWorkflow->getContext();
        }

        // 完成或中断当前工作流
        if (previousWorkflow) {
            previousWorkflow->finish();
        }

        // 启动新工作流
        if (context) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                currentWorkflow_ = workflow;
            }
            currentWorkflow_->start(context);
        } else {
            startWorkflow(name);
        }

        return true;
    }

    void interrupt() override {
        auto workflow = getCurrentWorkflow();
        if (workflow) {
            LOG_INFO("[WorkflowManager] Interrupting workflow: " + workflow->getName());
            workflow->interrupt();
            std::lock_guard<std::mutex> lock(mutex_);
            currentWorkflow_ = nullptr;
        }
    }

    void cancel() override {
        auto workflow = getCurrentWorkflow();
        if (workflow) {
            LOG_INFO("[WorkflowManager] Canceling workflow: " + workflow->getName());
            workflow->cancel();
            std::lock_guard<std::mutex> lock(mutex_);
            currentWorkflow_ = nullptr;
        }
    }

    // ========== 工作流队列管理 ==========
    void enqueueWorkflow(const WorkflowTask& task) override {
        std::lock_guard<std::mutex> lock(mutex_);

        WorkflowTask finalTask = task;
        if (finalTask.taskId.empty()) {
            finalTask.taskId = IdGenerator::generate();
        }

        if (finalTask.timeoutMs == 0) {
            finalTask.timeoutMs = defaultTimeout_;
        }

        taskQueue_.push(finalTask);
        LOG_INFO("[WorkflowManager] Enqueued task: " + finalTask.taskId +
                  " for workflow: " + finalTask.workflowName);

        // 如果队列处理线程未运行，启动它
        if (!processingThreadRunning_) {
            startQueueProcessing();
        }
    }

    void dequeueWorkflow(const std::string& taskId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<WorkflowTask> newQueue;

        while (!taskQueue_.empty()) {
            auto task = taskQueue_.front();
            taskQueue_.pop();
            if (task.taskId == taskId) {
                LOG_INFO("[WorkflowManager] Dequeued task: " + taskId);
            } else {
                newQueue.push(task);
            }
        }

        taskQueue_ = newQueue;
    }

    std::vector<WorkflowTask> getQueue() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<WorkflowTask> result;
        std::queue<WorkflowTask> tempQueue = taskQueue_;

        while (!tempQueue.empty()) {
            result.push_back(tempQueue.front());
            tempQueue.pop();
        }

        return result;
    }

    void processQueue() override {
        std::unique_lock<std::mutex> lock(mutex_);
        queueCV_.notify_one();
    }

    bool isQueueProcessing() const override {
        return queueProcessing_;
    }

    size_t getQueueSize() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return taskQueue_.size();
    }

    // ========== 消息处理 - 转发消息给当前工作流 ==========
    bool handleMessage(const IMessage& message) override {
        LOG_INFO("[WorkflowManager] Received message: " + message.getTopic());

        // 转发给当前工作流
        auto workflow = getCurrentWorkflow();
        if (workflow && (workflow->getState() == WorkflowState::RUNNING ||
                        workflow->getState() == WorkflowState::EXECUTING)) {
            workflow->handleMessage(message);
            return true;
        }
        LOG_WARNING("[WorkflowManager] No active workflow to handle message");
        return false;
    }

    // ========== 资源管理 ==========
    void cleanupAllResources() override {
        size_t count = resourceManager_->cleanupAll();
        LOG_INFO("[WorkflowManager] Cleaned up " + std::to_string(count) + " resources");
    }

    // ========== 状态查询 ==========
    std::shared_ptr<IWorkflow> getCurrentWorkflow() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentWorkflow_;
    }

    std::string getStatus() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"current_workflow\": "
           << (currentWorkflow_ ? "\"" + currentWorkflow_->getName() + "\"" : "null") << ",\n";
        ss << "  \"workflow_state\": "
           << (currentWorkflow_ ? "\"" + workflowStateToString(currentWorkflow_->getState()) + "\"" : "null") << ",\n";
        ss << "  \"registered_workflows\": [";
        bool first = true;
        for (const auto& pair : workflows_) {
            if (!first) ss << ", ";
            ss << "\"" << pair.first << "\"";
            first = false;
        }
        ss << "],\n";
        ss << "  \"resources_count\": " << resourceManager_->getAllResources().size() << ",\n";
        ss << "  \"queue_size\": " << taskQueue_.size() << ",\n";
        ss << "  \"queue_processing\": " << (queueProcessing_ ? "true" : "false") << "\n";
        ss << "}";
        return ss.str();
    }

    std::shared_ptr<IResourceManager> getResourceManager() const override {
        return resourceManager_;
    }

    // ========== 执行结果管理 ==========
    WorkflowExecutionResult getLastExecutionResult() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return executionResults_.empty() ? WorkflowExecutionResult{} : executionResults_.back();
    }

    WorkflowExecutionResult getExecutionResult(const std::string& taskId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& result : executionResults_) {
            if (result.taskId == taskId) {
                return result;
            }
        }
        return WorkflowExecutionResult{};
    }

    std::vector<WorkflowExecutionResult> getAllExecutionResults() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return executionResults_;
    }

    // ========== 超时控制 ==========
    void setDefaultTimeout(long timeoutMs) override {
        std::lock_guard<std::mutex> lock(mutex_);
        defaultTimeout_ = timeoutMs;
        LOG_INFO("[WorkflowManager] Default timeout set to " + std::to_string(timeoutMs) + " ms");
    }

    long getDefaultTimeout() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return defaultTimeout_;
    }

    // ========== 持久化支持 ==========
    bool saveWorkflowExecution(const std::string& workflowName,
                                         WorkflowState finalState,
                                         bool success,
                                         const std::string& errorMessage,
                                         int64_t durationMs) override {
        if (!persistence_) {
            // 如果持久化未初始化，静默返回成功
            return true;
        }

        WorkflowRecord record;
        record.workflowId = currentExecutionId_;
        record.workflowName = workflowName;
        record.state = finalState;
        record.startTime = TimeUtils::getCurrentMs() - durationMs;
        record.endTime = TimeUtils::getCurrentMs();
        record.duration = durationMs;
        record.success = success;
        record.errorMessage = errorMessage;
        record.retryCount = 0;

        bool result = persistence_->saveWorkflow(record);
        if (result) {
            LOG_INFO("[WorkflowManager] Saved execution record for: " + workflowName);
        } else {
            LOG_ERROR("[WorkflowManager] Failed to save execution record");
        }
        return result;
    }

    bool saveWorkflowDefinition(const std::string& workflowId,
                                           const std::string& definitionJson) override {
        if (!persistence_) {
            return true;
        }
        return persistence_->saveWorkflowDefinition(workflowId, definitionJson);
    }

    std::string getWorkflowDefinition(const std::string& workflowId) const override {
        if (!persistence_) {
            return "";
        }
        return persistence_->getWorkflowDefinition(workflowId);
    }

    std::vector<std::string> getWorkflowExecutions(const std::string& workflowName) const override {
        if (!persistence_) {
            return std::vector<std::string>();
        }

        std::vector<std::string> executionIds;
        auto records = persistence_->getWorkflowsByName(workflowName);
        for (const auto& record : records) {
            executionIds.push_back(record.workflowId);
        }
        return executionIds;
    }

    /**
     * @brief 启用持久化功能
     * @param databasePath 数据库文件路径
     * @return 是否启用成功
     */
    bool enablePersistence(const std::string& databasePath) {
        persistence_ = &SqliteWorkflowPersistence::getInstance();
        if (!persistence_->initialize(databasePath)) {
            LOG_ERROR("[WorkflowManager] Failed to initialize persistence");
            persistence_ = nullptr;
            return false;
        }
        LOG_INFO("[WorkflowManager] Persistence enabled with database: " + databasePath);
        return true;
    }

    /**
     * @brief 禁用持久化功能
     */
    void disablePersistence() {
        if (persistence_) {
            persistence_->close();
            persistence_ = nullptr;
            LOG_INFO("[WorkflowManager] Persistence disabled");
        }
    }

    /**
     * @brief 获取持久化状态
     * @return 是否已启用持久化
     */
    bool isPersistenceEnabled() const {
        return persistence_ != nullptr;
    }

private:
    // 启动队列处理线程
    void startQueueProcessing() {
        processingThreadRunning_ = true;
        processingThread_ = std::thread(&WorkflowManager::queueProcessingLoop, this);
    }

    // 停止队列处理线程
    void stopQueueProcessing() {
        processingThreadRunning_ = false;
        queueCV_.notify_all();
        if (processingThread_.joinable()) {
            processingThread_.join();
        }
    }

    // 队列处理循环
    void queueProcessingLoop() {
        while (processingThreadRunning_) {
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待任务
            queueCV_.wait(lock, [this]() {
                return !taskQueue_.empty() || !processingThreadRunning_;
            });

            if (!processingThreadRunning_) {
                break;
            }

            // 检查是否有当前工作流正在运行
            if (currentWorkflow_ && (
                currentWorkflow_->getState() == WorkflowState::RUNNING ||
                currentWorkflow_->getState() == WorkflowState::EXECUTING)) {
                // 等待当前工作流完成
                continue;
            }

            // 取出下一个任务
            if (!taskQueue_.empty()) {
                auto task = taskQueue_.front();
                taskQueue_.pop();
                queueProcessing_ = true;
                lock.unlock();

                // 执行任务（在锁外执行）
                executeTask(task);

                lock.lock();
                queueProcessing_ = false;
            }
        }
    }

    // 执行单个任务
    void executeTask(const WorkflowTask& task) {
        // 创建或使用传入的上下文
        std::shared_ptr<IWorkflowContext> context = task.context;
        if (!context) {
            context = std::make_shared<WorkflowContext>(resourceManager_);
        }

        // 生成执行ID
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentExecutionId_ = "exec_" + std::to_string(TimeUtils::getCurrentMs()) + "_" + task.taskId;
        }

        // 获取工作流
        auto workflow = getWorkflow(task.workflowName);
        if (!workflow) {
            LOG_ERROR("[WorkflowManager] Workflow '" + task.workflowName + "' not found for task: " + task.taskId);
            if (task.onError) {
                task.onError("Workflow not found");
            }
            return;
        }

        // 启动工作流
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentWorkflow_ = workflow;
        }
        workflow->start(context);

        // 记录开始时间
        long startTime = TimeUtils::getCurrentMs();

        // 执行工作流
        auto future = workflow->execute();
        Any result;

        // 等待执行完成或超时
        if (task.timeoutMs > 0) {
            auto status = future.wait_for(std::chrono::milliseconds(task.timeoutMs));
            if (status == std::future_status::timeout) {
                LOG_WARNING("[WorkflowManager] Task " + task.taskId + " timed out");
                workflow->interrupt();
                if (task.onError) {
                    task.onError("Execution timeout");
                }
                long endTime = TimeUtils::getCurrentMs();
                recordExecutionResult(task, result, WorkflowState::INTERRUPTED,
                                   startTime, endTime, "Timeout");
                saveWorkflowExecution(task.workflowName, WorkflowState::INTERRUPTED,
                                    false, "Timeout", endTime - startTime);
                return;
            }
        }

        try {
            result = future.get();
            if (task.onComplete) {
                task.onComplete(result);
            }
            long endTime = TimeUtils::getCurrentMs();
            recordExecutionResult(task, result, workflow->getState(),
                               startTime, endTime, "");
            bool success = (workflow->getState() == WorkflowState::COMPLETED);
            saveWorkflowExecution(task.workflowName, workflow->getState(),
                                success, "", endTime - startTime);
            LOG_INFO("[WorkflowManager] Task " + task.taskId + " completed successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("[WorkflowManager] Task " + task.taskId + " failed: " + std::string(e.what()));
            if (task.onError) {
                task.onError(e.what());
            }
            long endTime = TimeUtils::getCurrentMs();
            recordExecutionResult(task, Any(), WorkflowState::ERROR,
                               startTime, endTime, e.what());
            saveWorkflowExecution(task.workflowName, WorkflowState::ERROR,
                                false, e.what(), endTime - startTime);
        }
    }

    // 记录执行结果
    void recordExecutionResult(const WorkflowTask& task, const Any& result,
                           WorkflowState state, long startTime, long endTime,
                           const std::string& errorMessage) {
        WorkflowExecutionResult execResult;
        execResult.taskId = task.taskId;
        execResult.workflowName = task.workflowName;
        execResult.result = result;
        execResult.finalState = state;
        execResult.startTime = startTime;
        execResult.endTime = endTime;
        execResult.executionTimeMs = endTime - startTime;
        execResult.errorMessage = errorMessage;

        std::lock_guard<std::mutex> lock(mutex_);
        executionResults_.push_back(execResult);

        // 限制历史记录数量（最多保留100条）
        if (executionResults_.size() > 100) {
            executionResults_.erase(executionResults_.begin());
        }
    }

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
