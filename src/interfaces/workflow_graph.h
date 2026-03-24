/**
 * @file workflow_graph.h
 * @brief 业务流程管理系统 - 工作流图接口
 *
 * 设计模式：
 * - 组合模式：节点和边组成图
 * - 迭代器模式：遍历图
 *
 * 面向对象：
 * - 封装：封装图的拓扑结构和遍历逻辑
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_GRAPH_H
#define WORKFLOW_SYSTEM_WORKFLOW_GRAPH_H

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <map>

namespace WorkflowSystem {

/**
 * @brief 节点类型枚举
 */
enum class NodeType {
    TASK,           // 任务节点
    CONDITION,      // 条件节点
    PARALLEL,       // 并行节点
    MERGE,          // 合并节点
    START,          // 开始节点
    END             // 结束节点
};

/**
 * @brief 节点状态枚举
 */
enum class NodeState {
    PENDING,        // 待执行
    RUNNING,        // 执行中
    COMPLETED,      // 已完成
    FAILED,         // 失败
    SKIPPED         // 跳过
};

/**
 * @brief 工作流节点接口
 */
class IWorkflowNode {
public:
    virtual ~IWorkflowNode() = default;

    /**
     * @brief 获取节点ID
     */
    virtual std::string getId() const = 0;

    /**
     * @brief 获取节点名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取节点类型
     */
    virtual NodeType getType() const = 0;

    /**
     * @brief 获取节点状态
     */
    virtual NodeState getState() const = 0;

    /**
     * @brief 设置节点状态
     */
    virtual void setState(NodeState state) = 0;

    /**
     * @brief 获取前置节点ID列表
     */
    virtual std::vector<std::string> getPredecessors() const = 0;

    /**
     * @brief 获取后置节点ID列表
     */
    virtual std::vector<std::string> getSuccessors() const = 0;

    /**
     * @brief 添加前置节点
     */
    virtual void addPredecessor(const std::string& nodeId) = 0;

    /**
     * @brief 添加后置节点
     */
    virtual void addSuccessor(const std::string& nodeId) = 0;

    /**
     * @brief 获取节点属性
     */
    virtual std::map<std::string, std::string> getAttributes() const = 0;

    /**
     * @brief 设置节点属性
     */
    virtual void setAttribute(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 获取属性值
     */
    virtual std::string getAttribute(const std::string& key,
                                 const std::string& defaultValue = "") const = 0;
};

/**
 * @brief 边接口
 */
class IWorkflowEdge {
public:
    virtual ~IWorkflowEdge() = default;

    /**
     * @brief 获取源节点ID
     */
    virtual std::string getSourceId() const = 0;

    /**
     * @brief 获取目标节点ID
     */
    virtual std::string getTargetId() const = 0;

    /**
     * @brief 获取条件（用于条件边）
     */
    virtual std::string getCondition() const = 0;

    /**
     * @brief 设置条件
     */
    virtual void setCondition(const std::string& condition) = 0;

    /**
     * @brief 获取边的权重
     */
    virtual int getWeight() const = 0;

    /**
     * @brief 设置边的权重
     */
    virtual void setWeight(int weight) = 0;
};

/**
 * @brief 工作流图接口
 */
class IWorkflowGraph {
public:
    virtual ~IWorkflowGraph() = default;

    /**
     * @brief 添加节点
     */
    virtual void addNode(std::shared_ptr<IWorkflowNode> node) = 0;

    /**
     * @brief 移除节点
     */
    virtual bool removeNode(const std::string& nodeId) = 0;

    /**
     * @brief 获取节点
     */
    virtual std::shared_ptr<IWorkflowNode> getNode(const std::string& nodeId) const = 0;

    /**
     * @brief 获取所有节点
     */
    virtual std::vector<std::shared_ptr<IWorkflowNode>> getAllNodes() const = 0;

    /**
     * @brief 添加边
     */
    virtual void addEdge(const std::string& sourceId,
                       const std::string& targetId,
                       const std::string& condition = "") = 0;

    /**
     * @brief 移除边
     */
    virtual bool removeEdge(const std::string& sourceId,
                         const std::string& targetId) = 0;

    /**
     * @brief 获取所有边
     */
    virtual std::vector<std::shared_ptr<IWorkflowEdge>> getAllEdges() const = 0;

    /**
     * @brief 检查是否为有向无环图（DAG）
     */
    virtual bool isDAG() const = 0;

    /**
     * @brief 拓扑排序
     * @return 按拓扑顺序排列的节点ID列表，如果图有环则返回空
     */
    virtual std::vector<std::string> topologicalSort() const = 0;

    /**
     * @brief 获取开始节点（没有前置节点的节点）
     */
    virtual std::vector<std::shared_ptr<IWorkflowNode>> getStartNodes() const = 0;

    /**
     * @brief 获取结束节点（没有后置节点的节点）
     */
    virtual std::vector<std::shared_ptr<IWorkflowNode>> getEndNodes() const = 0;

    /**
     * @brief 检测环
     * @return 图中所有的环，每个环由节点ID列表组成
     */
    virtual std::vector<std::vector<std::string>> detectCycles() const = 0;

    /**
     * @brief 清空图
     */
    virtual void clear() = 0;

    /**
     * @brief 获取节点数量
     */
    virtual size_t getNodeCount() const = 0;

    /**
     * @brief 获取边数量
     */
    virtual size_t getEdgeCount() const = 0;

    /**
     * @brief 设置图名称
     */
    virtual void setName(const std::string& name) = 0;

    /**
     * @brief 获取图名称
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief 图验证错误
 */
struct GraphValidationError {
    std::string message;          // 错误消息
    std::string nodeId;          // 相关节点ID
    std::string suggestion;       // 修复建议
};

/**
 * @brief 图验证器接口
 */
class IGraphValidator {
public:
    virtual ~IGraphValidator() = default;

    /**
     * @brief 验证图
     */
    virtual std::vector<GraphValidationError> validate(
        const IWorkflowGraph& graph) const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_GRAPH_H
