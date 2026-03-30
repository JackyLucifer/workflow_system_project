/**
 * @file workflow_orchestrator.h
 * @brief 业务流程管理系统 - 工作流编排器接口
 *
 * 设计模式：
 * - 策略模式：不同的执行策略（顺序、并行）
 * - 状态模式：工作流状态转换
 *
 * 面向对象：
 * - 封装：封装工作流执行逻辑和状态管理
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_H
#define WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_H

#include "workflow_system/interfaces/workflow_graph.h"
#include "workflow_system/interfaces/workflow.h"
#include <functional>
#include <memory>
#include <vector>
#include <map>

namespace WorkflowSystem {

/**
 * @brief 工作流编排状态
 */
enum class OrchestrationState {
    IDLE,           // 空闲
    RUNNING,        // 运行中
    PAUSED,         // 暂停
    COMPLETED,      // 已完成
    FAILED          // 失败
};

/**
 * @brief 执行策略
 */
enum class ExecutionStrategy {
    SEQUENTIAL,     // 顺序执行
    PARALLEL,        // 并行执行
    CONDITIONAL      // 条件执行
};

/**
 * @brief 节点执行结果
 */
struct NodeExecutionResult {
    std::string nodeId;              // 节点ID
    bool success;                   // 是否成功
    std::string errorMessage;         // 错误消息
    std::map<std::string, std::string> output;  // 输出数据
    long executionTimeMs;           // 执行时间（毫秒）
};

/**
 * @brief 工作流执行上下文
 */
class IOrchestrationContext {
public:
    virtual ~IOrchestrationContext() = default;

    /**
     * @brief 获取节点执行结果
     */
    virtual NodeExecutionResult getNodeResult(const std::string& nodeId) const = 0;

    /**
     * @brief 设置节点执行结果
     */
    virtual void setNodeResult(const std::string& nodeId,
                            const NodeExecutionResult& result) = 0;

    /**
     * @brief 获取所有变量
     */
    virtual std::map<std::string, std::string> getAllVariables() const = 0;

    /**
     * @brief 设置变量
     */
    virtual void setVariable(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 获取变量
     */
    virtual std::string getVariable(const std::string& key,
                                const std::string& defaultValue = "") const = 0;

    /**
     * @brief 获取工作流图
     */
    virtual std::shared_ptr<IWorkflowGraph> getGraph() const = 0;

    /**
     * @brief 获取当前状态
     */
    virtual OrchestrationState getState() const = 0;
};

/**
 * @brief 节点执行器接口
 */
class INodeExecutor {
public:
    virtual ~INodeExecutor() = default;

    /**
     * @brief 执行节点
     */
    virtual NodeExecutionResult execute(
        const std::shared_ptr<IWorkflowNode>& node,
        IOrchestrationContext* context) = 0;
};

/**
 * @brief 工作流编排器接口
 */
class IWorkflowOrchestrator {
public:
    virtual ~IWorkflowOrchestrator() = default;

    /**
     * @brief 设置工作流图
     */
    virtual void setGraph(std::shared_ptr<IWorkflowGraph> graph) = 0;

    /**
     * @brief 获取工作流图
     */
    virtual std::shared_ptr<IWorkflowGraph> getGraph() const = 0;

    /**
     * @brief 设置节点执行器
     */
    virtual void setNodeExecutor(std::shared_ptr<INodeExecutor> executor) = 0;

    /**
     * @brief 执行工作流
     */
    virtual bool execute() = 0;

    /**
     * @brief 暂停工作流
     */
    virtual void pause() = 0;

    /**
     * @brief 恢复工作流
     */
    virtual void resume() = 0;

    /**
     * @brief 终止工作流
     */
    virtual void abort() = 0;

    /**
     * @brief 获取当前状态
     */
    virtual OrchestrationState getState() const = 0;

    /**
     * @brief 获取执行上下文
     */
    virtual IOrchestrationContext* getContext() const = 0;

    /**
     * @brief 获取所有节点执行结果
     */
    virtual std::vector<NodeExecutionResult> getAllResults() const = 0;

    /**
     * @brief 设置执行策略
     */
    virtual void setExecutionStrategy(ExecutionStrategy strategy) = 0;

    /**
     * @brief 获取执行策略
     */
    virtual ExecutionStrategy getExecutionStrategy() const = 0;

    /**
     * @brief 设置变量
     */
    virtual void setVariable(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 获取变量
     */
    virtual std::string getVariable(const std::string& key,
                                const std::string& defaultValue = "") const = 0;

    /**
     * @brief 获取进度（0-100）
     */
    virtual int getProgress() const = 0;

    /**
     * @brief 设置进度回调
     */
    using ProgressCallback = std::function<void(int)>;
    virtual void setProgressCallback(ProgressCallback callback) = 0;

    /**
     * @brief 设置完成回调
     */
    using CompletionCallback = std::function<void(bool, const std::string&)>;
    virtual void setCompletionCallback(CompletionCallback callback) = 0;

    /**
     * @brief 添加工作流观察者
     */
    virtual void addObserver(std::shared_ptr<IWorkflowObserver> observer) = 0;

    /**
     * @brief 移除工作流观察者
     */
    virtual void removeObserver(std::shared_ptr<IWorkflowObserver> observer) = 0;

    /**
     * @brief 清除所有观察者
     */
    virtual void clearObservers() = 0;

    /**
     * @brief 重置编排器
     */
    virtual void reset() = 0;
};

/**
 * @brief 默认节点执行器工厂
 */
class DefaultNodeExecutorFactory {
public:
    /**
     * @brief 创建默认节点执行器
     */
    static std::shared_ptr<INodeExecutor> create();
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_ORCHESTRATOR_H
