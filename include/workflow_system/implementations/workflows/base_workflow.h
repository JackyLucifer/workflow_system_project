/**
 * @file base_workflow.h
 * @brief 业务流程管理系统 - 工作流基类
 *
 * 设计模式：
 * - 模板方法模式：定义算法骨架，子类实现具体步骤
 * - 观察者模式：支持观察工作流状态变化
 *
 * 面向对象：
 * - 封装：管理工作流的状态和生命周期
 * - 继承：子类继承通用行为
 * - 多态：通过虚函数实现不同的行为
 */

#ifndef WORKFLOW_SYSTEM_BASE_WORKFLOW_H
#define WORKFLOW_SYSTEM_BASE_WORKFLOW_H

#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <future>
#include <atomic>
#include <condition_variable>
#include "interfaces/workflow.h"
#include "interfaces/resource_manager.h"
#include "core/types.h"
#include "core/logger.h"
#include "core/any.h"

namespace WorkflowSystem {

/**
 * @brief 可观察基类
 *
 * 设计模式：观察者模式
 * 封装观察者管理逻辑
 */
class Observable {
public:
    void addObserver(const std::shared_ptr<IWorkflowObserver>& observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }

    void removeObserver(const std::shared_ptr<IWorkflowObserver>& observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(observers_.begin(), observers_.end(),
            [&observer](const std::weak_ptr<IWorkflowObserver>& weak) {
                auto locked = weak.lock();
                return !locked || locked == observer;
            });
        observers_.erase(it, observers_.end());
    }

protected:
    void notifyStarted(const std::string& workflowName) {
        for (auto& weakObserver : observers_) {
            if (auto observer = weakObserver.lock()) {
                observer->onWorkflowStarted(workflowName);
            }
        }
    }

    void notifyFinished(const std::string& workflowName) {
        for (auto& weakObserver : observers_) {
            if (auto observer = weakObserver.lock()) {
                observer->onWorkflowFinished(workflowName);
            }
        }
    }

    void notifyInterrupted(const std::string& workflowName) {
        for (auto& weakObserver : observers_) {
            if (auto observer = weakObserver.lock()) {
                observer->onWorkflowInterrupted(workflowName);
            }
        }
    }

    void notifyError(const std::string& workflowName, const std::string& error) {
        for (auto& weakObserver : observers_) {
            if (auto observer = weakObserver.lock()) {
                observer->onWorkflowError(workflowName, error);
            }
        }
    }

private:
    std::vector<std::weak_ptr<IWorkflowObserver>> observers_;
    std::mutex mutex_;
};

/**
 * @brief 工作流抽象基类
 *
 * 设计模式：模板方法模式
 * 定义工作流的生命周期和算法骨架
 */
class BaseWorkflow : public IWorkflow, public Observable {
public:
    BaseWorkflow(const std::string& name, std::shared_ptr<IResourceManager> resourceManager)
        : name_(name), resourceManager_(resourceManager), state_(WorkflowState::IDLE),
          stopFlag_(false), pausedFlag_(false) {}

    virtual ~BaseWorkflow() {
        stop();
        if (executionThread_.joinable()) {
            executionThread_.join();
        }
    }

    // IWorkflow 接口实现
    std::string getName() const override { return name_; }
    WorkflowState getState() const override { return state_; }

    void setContext(std::shared_ptr<IWorkflowContext> context) override {
        context_ = context;
    }

    std::shared_ptr<IWorkflowContext> getContext() const override {
        return context_;
    }

    // 观察者管理
    void addObserver(std::shared_ptr<IWorkflowObserver> observer) override {
        Observable::addObserver(observer);
    }

    void removeObserver(std::shared_ptr<IWorkflowObserver> observer) override {
        Observable::removeObserver(observer);
    }

    // 消息处理器设置
    void setMessageHandler(MessageHandler handler) override {
        messageHandler_ = handler;
    }

    // IWorkflowObserver 接口实现（空实现，使用(void)消除警告）
    void onWorkflowStarted(const std::string&) override { /* 默认空实现 */ }
    void onWorkflowFinished(const std::string&) override { /* 默认空实现 */ }
    void onWorkflowInterrupted(const std::string&) override { /* 默认空实现 */ }
    void onWorkflowError(const std::string&, const std::string&) override { /* 默认空实现 */ }

    // 模板方法 - 定义工作流生命周期算法骨架
    void start(std::shared_ptr<IWorkflowContext> context) override {
        if (context) {
            context_ = context;
        } else if (!context_) {
            LOG_WARNING("[BaseWorkflow] Context not provided");
            return;
        }

        setState(WorkflowState::INITIALIZING);
        LOG_INFO("[" + name_ + "] Initializing workflow...");

        // 钩子方法 - 子类实现
        onStart();

        setState(WorkflowState::READY);
        notifyStarted(name_);

        LOG_INFO("[" + name_ + "] Workflow ready");
    }

    void handleMessage(const IMessage& message) override {
        // 检查状态，只允许 RUNNING 或 EXECUTING 状态下处理消息
        if (state_ != WorkflowState::RUNNING && state_ != WorkflowState::EXECUTING) {
            LOG_WARNING("[" + name_ + "] Workflow not in running state (state: " +
                       workflowStateToString(state_) + "), ignoring message");
            return;
        }

        // 优先使用自定义消息处理器
        if (messageHandler_) {
            messageHandler_(message);
        }

        // 钩子方法 - 子类实现
        onMessage(message);
    }

    // 执行工作流主体函数（在新线程中）
    // 返回 future，用于获取执行结果
    std::future<Any> execute() override {
        // 只有 READY 状态下才能执行
        if (state_ != WorkflowState::READY) {
            LOG_WARNING("[" + name_ + "] Workflow not ready (state: " +
                       workflowStateToString(state_) + "), cannot execute");
            // 返回一个已经设置错误结果的 future
            std::promise<Any> promise;
            promise.set_value(Any());
            return promise.get_future();
        }

        // 重置停止和暂停标志
        stopFlag_ = false;
        pausedFlag_ = false;

        // 创建 promise 用于返回结果
        auto promise = std::make_shared<std::promise<Any>>();

        // 启动执行线程
        {
            std::lock_guard<std::mutex> lock(threadMutex_);
            if (executionThread_.joinable()) {
                executionThread_.join();
            }
            executionThread_ = std::thread(&BaseWorkflow::executeInternal, this, promise);
        }

        return promise->get_future();
    }

    // 获取执行结果（阻塞等待）
    Any getExecutionResult() const {
        if (executionResult_.has_value()) {
            return executionResult_;
        }
        return Any();
    }

    // 检查执行是否完成
    bool isExecutionCompleted() const {
        return executionResult_.has_value();
    }

    void interrupt() override {
        // 只有 RUNNING 或 EXECUTING 状态下才能中断
        if (state_ != WorkflowState::RUNNING && state_ != WorkflowState::EXECUTING) {
            LOG_WARNING("[" + name_ + "] Workflow not in running state (state: " +
                       workflowStateToString(state_) + "), cannot interrupt");
            return;
        }

        stop();
        setState(WorkflowState::INTERRUPTED);
        LOG_INFO("[" + name_ + "] Interrupting workflow...");

        onInterrupt();
        notifyInterrupted(name_);
    }

    void pause() {
        // 只有 RUNNING 或 EXECUTING 状态下才能暂停
        if (state_ != WorkflowState::RUNNING && state_ != WorkflowState::EXECUTING) {
            LOG_WARNING("[" + name_ + "] Workflow not in running state (state: " +
                       workflowStateToString(state_) + "), cannot pause");
            return;
        }

        if (state_ == WorkflowState::PAUSED || state_ == WorkflowState::SUSPENDED) {
            LOG_WARNING("[" + name_ + "] Workflow already paused");
            return;
        }

        pausedFlag_ = true;
        setState(WorkflowState::PAUSED);
        LOG_INFO("[" + name_ + "] Pausing workflow...");

        // 钩子方法 - 子类可以重写实现暂停逻辑
        onPause();
    }

    void resume() {
        // 只有 PAUSED 状态下才能恢复
        if (state_ != WorkflowState::PAUSED) {
            LOG_WARNING("[" + name_ + "] Workflow not paused (state: " +
                       workflowStateToString(state_) + "), cannot resume");
            return;
        }

        pausedFlag_ = false;
        pauseCV_.notify_all();
        setState(WorkflowState::EXECUTING);
        LOG_INFO("[" + name_ + "] Resuming workflow...");

        // 钩子方法 - 子类可以重写实现恢复逻辑
        onResume();
    }

    void suspend() {
        // 只有 RUNNING 或 EXECUTING 状态下才能挂起
        if (state_ != WorkflowState::RUNNING && state_ != WorkflowState::EXECUTING) {
            LOG_WARNING("[" + name_ + "] Workflow not in running state (state: " +
                       workflowStateToString(state_) + "), cannot suspend");
            return;
        }

        if (state_ == WorkflowState::PAUSED || state_ == WorkflowState::SUSPENDED) {
            LOG_WARNING("[" + name_ + "] Workflow already suspended");
            return;
        }

        pausedFlag_ = true;
        setState(WorkflowState::SUSPENDED);
        LOG_INFO("[" + name_ + "] Suspending workflow...");

        // 钩子方法 - 子类可以重写实现挂起逻辑
        onSuspend();
    }

    void cancel() override {
        // 可以从任何非完成状态取消
        if (state_ == WorkflowState::COMPLETED || state_ == WorkflowState::INTERRUPTED ||
            state_ == WorkflowState::TERMINATING || state_ == WorkflowState::CANCELED) {
            LOG_WARNING("[" + name_ + "] Workflow cannot be cancelled (state: " +
                       workflowStateToString(state_) + ")");
            return;
        }

        stop();
        setState(WorkflowState::CANCELED);
        LOG_INFO("[" + name_ + "] Cancelling workflow...");

        // 钩子方法 - 子类可以重写实现取消逻辑
        onCancel();
    }

    void finish() override {
        // 只有 RUNNING 或 EXECUTING 状态下才能完成
        if (state_ != WorkflowState::RUNNING && state_ != WorkflowState::EXECUTING) {
            LOG_WARNING("[" + name_ + "] Workflow not in running state (state: " +
                       workflowStateToString(state_) + "), cannot finish");
            return;
        }

        stop();
        setState(WorkflowState::TERMINATING);
        LOG_INFO("[" + name_ + "] Terminating workflow...");

        onFinalize();
        setState(WorkflowState::COMPLETED);
        notifyFinished(name_);
    }

    void error(const std::string& errorMsg) override {
        stop();
        setState(WorkflowState::ERROR);
        LOG_ERROR("[" + name_ + "] Error: " + errorMsg);
        notifyError(name_, errorMsg);
    }

    // 状态查询辅助方法
    bool isRunning() const {
        return state_ == WorkflowState::RUNNING || state_ == WorkflowState::EXECUTING;
    }

    bool isPaused() const {
        return state_ == WorkflowState::PAUSED;
    }

    bool isSuspended() const {
        return state_ == WorkflowState::SUSPENDED;
    }

    bool isCompleted() const {
        return state_ == WorkflowState::COMPLETED;
    }

    bool isIdle() const {
        return state_ == WorkflowState::IDLE;
    }

    bool isError() const {
        return state_ == WorkflowState::ERROR;
    }

protected:
    // 钩子方法 - 子类必须实现
    virtual void onStart() {}
    virtual void onMessage(const IMessage& /*message*/) {}
    virtual void onInterrupt() {}
    virtual void onFinalize() {}

    // 执行钩子方法 - 子类实现主体逻辑，返回执行结果
    virtual Any onExecute() = 0;

    // 可选钩子方法 - 子类可以选择重写
    virtual void onPause() {}
    virtual void onResume() {}
    virtual void onSuspend() {}
    virtual void onCancel() {}

    // 状态设置辅助方法
    void setState(WorkflowState newState) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        WorkflowState oldState = state_;

        // 验证状态转换是否合法
        if (!isValidStateTransition(oldState, newState)) {
            LOG_WARNING("[" + name_ + "] Invalid state transition: " +
                       workflowStateToString(oldState) + " -> " + workflowStateToString(newState));
            return;
        }

        state_ = newState;
        LOG_INFO("[" + name_ + "] State changed: " +
                  workflowStateToString(oldState) + " -> " + workflowStateToString(newState));
    }

    // 检查是否应该停止执行
    bool shouldStop() const {
        return stopFlag_;
    }

    // 检查是否暂停
    bool isPausedFlag() const {
        return pausedFlag_;
    }

    // 验证状态转换是否合法
    bool isValidStateTransition(WorkflowState from, WorkflowState to) {
        // 允许任何状态保持不变
        if (from == to) {
            return true;
        }

        // 定义合法的状态转换规则
        switch (from) {
            case WorkflowState::IDLE:
                return to == WorkflowState::INITIALIZING;

            case WorkflowState::INITIALIZING:
                return to == WorkflowState::READY ||
                       to == WorkflowState::ERROR;

            case WorkflowState::READY:
                return to == WorkflowState::RUNNING ||
                       to == WorkflowState::EXECUTING ||
                       to == WorkflowState::INTERRUPTED ||
                       to == WorkflowState::CANCELED;

            case WorkflowState::RUNNING:
            case WorkflowState::EXECUTING:
                return to == WorkflowState::PAUSED ||
                       to == WorkflowState::SUSPENDED ||
                       to == WorkflowState::INTERRUPTED ||
                       to == WorkflowState::CANCELED ||
                       to == WorkflowState::COMPLETED ||
                       to == WorkflowState::ERROR ||
                       to == WorkflowState::TERMINATING;

            case WorkflowState::PAUSED:
                return to == WorkflowState::EXECUTING ||
                       to == WorkflowState::INTERRUPTED ||
                       to == WorkflowState::CANCELED;

            case WorkflowState::SUSPENDED:
                return to == WorkflowState::EXECUTING ||
                       to == WorkflowState::INTERRUPTED ||
                       to == WorkflowState::CANCELED;

            case WorkflowState::TERMINATING:
                return to == WorkflowState::COMPLETED ||
                       to == WorkflowState::ERROR;

            case WorkflowState::INTERRUPTED:
            case WorkflowState::COMPLETED:
            case WorkflowState::ERROR:
            case WorkflowState::CANCELED:
            case WorkflowState::FAILED:
            case WorkflowState::ABORTED:
                // 终态不允许转换到其他状态
                return false;

            default:
                return false;
        }
    }

    // 等待恢复（在暂停时使用）
    void waitForResume() {
        std::unique_lock<std::mutex> lock(pauseMutex_);
        pauseCV_.wait(lock, [this]() { return !pausedFlag_ || stopFlag_; });
    }

    std::string name_;
    std::shared_ptr<IResourceManager> resourceManager_;
    std::shared_ptr<IWorkflowContext> context_;
    WorkflowState state_;
    MessageHandler messageHandler_;
    mutable std::mutex stateMutex_;

private:
    // 执行线程相关
    std::thread executionThread_;
    std::mutex threadMutex_;
    std::atomic<bool> stopFlag_;
    std::atomic<bool> pausedFlag_;
    std::mutex pauseMutex_;
    std::condition_variable pauseCV_;
    Any executionResult_;

    // 停止执行线程
    void stop() {
        stopFlag_ = true;
        pausedFlag_ = false;
        pauseCV_.notify_all();

        // 等待线程结束
        std::lock_guard<std::mutex> lock(threadMutex_);
        if (executionThread_.joinable()) {
            executionThread_.join();
        }
    }

    // 内部执行方法（在线程中运行）
    void executeInternal(std::shared_ptr<std::promise<Any>> promise) {
        try {
            setState(WorkflowState::EXECUTING);
            LOG_INFO("[" + name_ + "] Starting execution in thread...");

            // 调用子类实现的执行方法，获取结果
            executionResult_ = onExecute();

            // 正常完成，自动转为 COMPLETED 状态
            setState(WorkflowState::COMPLETED);
            LOG_INFO("[" + name_ + "] Execution completed");

            // 设置结果到 promise
            promise->set_value(executionResult_);

            // 通知观察者
            notifyFinished(name_);

            // 清理
            onFinalize();
        } catch (const std::runtime_error& e) {
            error(std::string("Runtime exception during execution: ") + e.what());
            promise->set_value(Any());
        } catch (const std::logic_error& e) {
            error(std::string("Logic exception during execution: ") + e.what());
            promise->set_value(Any());
        } catch (const std::exception& e) {
            error(std::string("Standard exception during execution: ") + e.what());
            promise->set_value(Any());
        } catch (...) {
            error("Unknown exception during execution");
            promise->set_value(Any());
        }
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_BASE_WORKFLOW_H
