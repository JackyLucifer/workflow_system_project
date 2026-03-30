/**
 * @file workflow_graph.h
 * @brief 业务流程管理系统 - 工作流图实现
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_GRAPH_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_GRAPH_IMPL_H

#include "workflow_system/interfaces/workflow_graph.h"
#include "workflow_system/core/logger.h"
#include <algorithm>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <functional>

namespace WorkflowSystem {

/**
 * @brief 工作流节点实现
 */
class WorkflowNode : public IWorkflowNode {
public:
    explicit WorkflowNode(const std::string& id, const std::string& name,
                       NodeType type = NodeType::TASK);

    std::string getId() const override;
    std::string getName() const override;
    NodeType getType() const override;
    NodeState getState() const override;
    void setState(NodeState state) override;

    std::vector<std::string> getPredecessors() const override;
    std::vector<std::string> getSuccessors() const override;

    void addPredecessor(const std::string& nodeId) override;
    void addSuccessor(const std::string& nodeId) override;

    std::map<std::string, std::string> getAttributes() const override;
    void setAttribute(const std::string& key, const std::string& value) override;
    std::string getAttribute(const std::string& key,
                          const std::string& defaultValue) const override;

    // 内部方法，用于修改前后关系
    void addPredecessorInternal(const std::string& nodeId);
    void addSuccessorInternal(const std::string& nodeId);
    void removePredecessorInternal(const std::string& nodeId);
    void removeSuccessorInternal(const std::string& nodeId);

private:
    std::string id_;
    std::string name_;
    NodeType type_;
    NodeState state_;
    mutable std::vector<std::string> predecessors_;
    mutable std::vector<std::string> successors_;
    std::map<std::string, std::string> attributes_;
};

/**
 * @brief 工作流边实现
 */
class WorkflowEdge : public IWorkflowEdge {
public:
    explicit WorkflowEdge(const std::string& sourceId, const std::string& targetId,
                        const std::string& condition = "");

    std::string getSourceId() const override;
    std::string getTargetId() const override;
    std::string getCondition() const override;
    void setCondition(const std::string& condition) override;

    int getWeight() const override;
    void setWeight(int weight) override;

private:
    std::string sourceId_;
    std::string targetId_;
    std::string condition_;
    int weight_;
};

/**
 * @brief 工作流图实现
 */
class WorkflowGraph : public IWorkflowGraph {
public:
    WorkflowGraph();

    void addNode(std::shared_ptr<IWorkflowNode> node) override;
    bool removeNode(const std::string& nodeId) override;
    std::shared_ptr<IWorkflowNode> getNode(const std::string& nodeId) const override;
    std::vector<std::shared_ptr<IWorkflowNode>> getAllNodes() const override;

    void addEdge(const std::string& sourceId, const std::string& targetId,
                const std::string& condition) override;
    bool removeEdge(const std::string& sourceId,
                  const std::string& targetId) override;
    std::vector<std::shared_ptr<IWorkflowEdge>> getAllEdges() const override;

    bool isDAG() const override;
    std::vector<std::string> topologicalSort() const override;
    std::vector<std::shared_ptr<IWorkflowNode>> getStartNodes() const override;
    std::vector<std::shared_ptr<IWorkflowNode>> getEndNodes() const override;
    std::vector<std::vector<std::string>> detectCycles() const override;

    void clear() override;
    size_t getNodeCount() const override;
    size_t getEdgeCount() const override;

    void setName(const std::string& name) override;
    std::string getName() const override;

private:
    std::string name_;
    std::map<std::string, std::shared_ptr<IWorkflowNode>> nodes_;
    std::vector<std::shared_ptr<IWorkflowEdge>> edges_;
};

/**
 * @brief 图验证器实现
 */
class GraphValidator : public IGraphValidator {
public:
    std::vector<GraphValidationError> validate(
        const IWorkflowGraph& graph) const override;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_GRAPH_IMPL_H
