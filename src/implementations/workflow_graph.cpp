/**
 * @file workflow_graph.cpp
 * @brief 业务流程管理系统 - 工作流图实现
 */

#include "workflow_system/implementations/workflow_graph.h"

namespace WorkflowSystem {

// ========== WorkflowNode 实现 ==========

WorkflowNode::WorkflowNode(const std::string& id, const std::string& name, NodeType type)
    : id_(id), name_(name), type_(type), state_(NodeState::PENDING) {}

std::string WorkflowNode::getId() const { return id_; }
std::string WorkflowNode::getName() const { return name_; }
NodeType WorkflowNode::getType() const { return type_; }
NodeState WorkflowNode::getState() const { return state_; }
void WorkflowNode::setState(NodeState state) { state_ = state; }

std::vector<std::string> WorkflowNode::getPredecessors() const {
    return predecessors_;
}

std::vector<std::string> WorkflowNode::getSuccessors() const {
    return successors_;
}

void WorkflowNode::addPredecessor(const std::string& nodeId) {
    predecessors_.push_back(nodeId);
}

void WorkflowNode::addSuccessor(const std::string& nodeId) {
    successors_.push_back(nodeId);
}

std::map<std::string, std::string> WorkflowNode::getAttributes() const {
    return attributes_;
}

void WorkflowNode::setAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value;
}

std::string WorkflowNode::getAttribute(const std::string& key,
                                      const std::string& defaultValue) const {
    auto it = attributes_.find(key);
    return it != attributes_.end() ? it->second : defaultValue;
}

void WorkflowNode::addPredecessorInternal(const std::string& nodeId) {
    predecessors_.push_back(nodeId);
}

void WorkflowNode::addSuccessorInternal(const std::string& nodeId) {
    successors_.push_back(nodeId);
}

void WorkflowNode::removePredecessorInternal(const std::string& nodeId) {
    auto it = std::find(predecessors_.begin(), predecessors_.end(), nodeId);
    if (it != predecessors_.end()) {
        predecessors_.erase(it);
    }
}

void WorkflowNode::removeSuccessorInternal(const std::string& nodeId) {
    auto it = std::find(successors_.begin(), successors_.end(), nodeId);
    if (it != successors_.end()) {
        successors_.erase(it);
    }
}

// ========== WorkflowEdge 实现 ==========

WorkflowEdge::WorkflowEdge(const std::string& sourceId, const std::string& targetId,
                          const std::string& condition)
    : sourceId_(sourceId), targetId_(targetId), condition_(condition),
      weight_(1) {}

std::string WorkflowEdge::getSourceId() const { return sourceId_; }
std::string WorkflowEdge::getTargetId() const { return targetId_; }
std::string WorkflowEdge::getCondition() const { return condition_; }

void WorkflowEdge::setCondition(const std::string& condition) {
    condition_ = condition;
}

int WorkflowEdge::getWeight() const { return weight_; }
void WorkflowEdge::setWeight(int weight) { weight_ = weight; }

// ========== WorkflowGraph 实现 ==========

WorkflowGraph::WorkflowGraph() : name_("WorkflowGraph") {}

void WorkflowGraph::addNode(std::shared_ptr<IWorkflowNode> node) {
    if (node) {
        nodes_[node->getId()] = node;
    }
}

bool WorkflowGraph::removeNode(const std::string& nodeId) {
    // 移除相关边
    for (auto it = edges_.begin(); it != edges_.end(); ) {
        if ((*it)->getSourceId() == nodeId ||
            (*it)->getTargetId() == nodeId) {
            it = edges_.erase(it);
        } else {
            ++it;
        }
    }

    // 从节点的前后关系中移除
    auto node = nodes_.find(nodeId);
    if (node != nodes_.end()) {
        // 从前置节点中移除
        for (const auto& predId : node->second->getPredecessors()) {
            auto pred = nodes_.find(predId);
            if (pred != nodes_.end()) {
                auto wfNode = static_cast<WorkflowNode*>(pred->second.get());
                wfNode->removeSuccessorInternal(nodeId);
            }
        }
        // 从后置节点中移除
        for (const auto& succId : node->second->getSuccessors()) {
            auto succ = nodes_.find(succId);
            if (succ != nodes_.end()) {
                auto wfNode = static_cast<WorkflowNode*>(succ->second.get());
                wfNode->removePredecessorInternal(nodeId);
            }
        }
        nodes_.erase(node);
        return true;
    }
    return false;
}

std::shared_ptr<IWorkflowNode> WorkflowGraph::getNode(const std::string& nodeId) const {
    auto it = nodes_.find(nodeId);
    return it != nodes_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<IWorkflowNode>> WorkflowGraph::getAllNodes() const {
    std::vector<std::shared_ptr<IWorkflowNode>> result;
    for (const auto& pair : nodes_) {
        result.push_back(pair.second);
    }
    return result;
}

void WorkflowGraph::addEdge(const std::string& sourceId, const std::string& targetId,
                           const std::string& condition) {
    auto source = nodes_.find(sourceId);
    auto target = nodes_.find(targetId);

    if (source != nodes_.end() && target != nodes_.end()) {
        auto edge = std::make_shared<WorkflowEdge>(sourceId, targetId, condition);
        edges_.push_back(edge);

        auto wfSource = static_cast<WorkflowNode*>(source->second.get());
        auto wfTarget = static_cast<WorkflowNode*>(target->second.get());
        wfSource->addSuccessorInternal(targetId);
        wfTarget->addPredecessorInternal(sourceId);
    }
}

bool WorkflowGraph::removeEdge(const std::string& sourceId,
                              const std::string& targetId) {
    for (auto it = edges_.begin(); it != edges_.end(); ++it) {
        if ((*it)->getSourceId() == sourceId &&
            (*it)->getTargetId() == targetId) {
            auto sourceNode = nodes_.find(sourceId);
            auto targetNode = nodes_.find(targetId);
            if (sourceNode != nodes_.end() && targetNode != nodes_.end()) {
                auto wfSource = static_cast<WorkflowNode*>(sourceNode->second.get());
                auto wfTarget = static_cast<WorkflowNode*>(targetNode->second.get());
                wfSource->removeSuccessorInternal(targetId);
                wfTarget->removePredecessorInternal(sourceId);
            }
            edges_.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<std::shared_ptr<IWorkflowEdge>> WorkflowGraph::getAllEdges() const {
    return edges_;
}

bool WorkflowGraph::isDAG() const {
    return detectCycles().empty();
}

std::vector<std::string> WorkflowGraph::topologicalSort() const {
    std::vector<std::string> result;
    std::map<std::string, int> inDegree;
    std::stack<std::string> zeroDegree;

    for (const auto& pair : nodes_) {
        inDegree[pair.first] = 0;
    }
    for (const auto& edge : edges_) {
        ++inDegree[edge->getTargetId()];
    }

    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            zeroDegree.push(pair.first);
        }
    }

    while (!zeroDegree.empty()) {
        std::string current = zeroDegree.top();
        zeroDegree.pop();
        result.push_back(current);

        auto node = nodes_.find(current);
        if (node != nodes_.end()) {
            for (const auto& succId : node->second->getSuccessors()) {
                --inDegree[succId];
                if (inDegree[succId] == 0) {
                    zeroDegree.push(succId);
                }
            }
        }
    }

    if (result.size() != nodes_.size()) {
        LOG_WARNING("[WorkflowGraph] Graph contains cycles, topological sort failed");
        return std::vector<std::string>();
    }

    return result;
}

std::vector<std::shared_ptr<IWorkflowNode>> WorkflowGraph::getStartNodes() const {
    std::vector<std::shared_ptr<IWorkflowNode>> result;
    for (const auto& pair : nodes_) {
        if (pair.second->getPredecessors().empty()) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<IWorkflowNode>> WorkflowGraph::getEndNodes() const {
    std::vector<std::shared_ptr<IWorkflowNode>> result;
    for (const auto& pair : nodes_) {
        if (pair.second->getSuccessors().empty()) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::vector<std::string>> WorkflowGraph::detectCycles() const {
    std::vector<std::vector<std::string>> cycles;
    std::map<std::string, int> state;
    std::vector<std::string> path;

    for (const auto& pair : nodes_) {
        state[pair.first] = 0;
    }

    std::function<void(const std::string&, std::vector<std::string>&)> dfs =
        [&](const std::string& nodeId, std::vector<std::string>& currentPath) {
            state[nodeId] = 1;
            currentPath.push_back(nodeId);

            auto node = nodes_.find(nodeId);
            if (node != nodes_.end()) {
                for (const auto& succId : node->second->getSuccessors()) {
                    if (state[succId] == 1) {
                        auto it = std::find(currentPath.begin(),
                                        currentPath.end(), succId);
                        std::vector<std::string> cycle(it, currentPath.end());
                        cycle.push_back(succId);
                        cycles.push_back(cycle);
                    } else if (state[succId] == 0) {
                        dfs(succId, currentPath);
                    }
                }
            }

            currentPath.pop_back();
            state[nodeId] = 2;
        };

    for (const auto& pair : nodes_) {
        if (state[pair.first] == 0) {
            std::vector<std::string> path;
            dfs(pair.first, path);
        }
    }

    return cycles;
}

void WorkflowGraph::clear() {
    nodes_.clear();
    edges_.clear();
}

size_t WorkflowGraph::getNodeCount() const { return nodes_.size(); }
size_t WorkflowGraph::getEdgeCount() const { return edges_.size(); }

void WorkflowGraph::setName(const std::string& name) { name_ = name; }
std::string WorkflowGraph::getName() const { return name_; }

// ========== GraphValidator 实现 ==========

std::vector<GraphValidationError> GraphValidator::validate(
    const IWorkflowGraph& graph) const {

    std::vector<GraphValidationError> errors;

    auto startNodes = graph.getStartNodes();
    if (startNodes.empty()) {
        GraphValidationError error;
        error.message = "No start nodes found (nodes without predecessors)";
        error.suggestion = "Add at least one start node or check for circular dependencies";
        errors.push_back(error);
    }

    auto endNodes = graph.getEndNodes();
    if (endNodes.empty()) {
        GraphValidationError error;
        error.message = "No end nodes found (nodes without successors)";
        error.suggestion = "Ensure at least one end node exists";
        errors.push_back(error);
    }

    auto cycles = graph.detectCycles();
    if (!cycles.empty()) {
        GraphValidationError error;
        error.message = "Graph contains " + std::to_string(cycles.size()) + " cycle(s)";
        std::string cycleList;
        for (size_t i = 0; i < cycles.size() && i < 3; ++i) {
            cycleList += "\n  Cycle " + std::to_string(i + 1) + ": ";
            for (const auto& nodeId : cycles[i]) {
                cycleList += nodeId + " -> ";
            }
            cycleList += cycles[i][0];
        }
        if (cycles.size() > 3) {
            cycleList += "\n  ... and " +
                        std::to_string(cycles.size() - 3) + " more cycle(s)";
        }
        error.suggestion = "Remove circular dependencies to make graph a DAG" + cycleList;
        errors.push_back(error);
    }

    auto allNodes = graph.getAllNodes();
    for (const auto& node : allNodes) {
        if (node->getPredecessors().empty() &&
            node->getType() != NodeType::START) {
            // Multiple start nodes is allowed
        }
    }

    return errors;
}

} // namespace WorkflowSystem
