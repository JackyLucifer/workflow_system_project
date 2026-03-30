/**
 * @file test_workflow_graph.cpp
 * @brief 工作流图模块测试用例
 */

#include "../include/workflow_system/interfaces/workflow_graph.h"
#include "../include/workflow_system/implementations/workflow_graph.h"
#include "../include/workflow_system/core/logger.h"
#include "test_framework.h"
#include <memory>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== WorkflowNode 测试 ==========
void testWorkflowNode_CreateAndGetId() {
    auto node = std::make_shared<WorkflowNode>("node1", "TestNode", NodeType::TASK);
    std::string id = node->getId();
    std::string name = node->getName();
    TEST_ASSERT_TRUE(id == "node1");
    TEST_ASSERT_TRUE(name == "TestNode");
    TEST_ASSERT_TRUE(node->getType() == NodeType::TASK);
}

void testWorkflowNode_SetAndGetState() {
    auto node = std::make_shared<WorkflowNode>("node1", "TestNode", NodeType::TASK);
    TEST_ASSERT_TRUE(node->getState() == NodeState::PENDING);

    node->setState(NodeState::RUNNING);
    TEST_ASSERT_TRUE(node->getState() == NodeState::RUNNING);

    node->setState(NodeState::COMPLETED);
    TEST_ASSERT_TRUE(node->getState() == NodeState::COMPLETED);
}

void testWorkflowNode_PredecessorsAndSuccessors() {
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);

    node2->addPredecessor(std::string("node1"));
    node2->addSuccessor(std::string("node3"));

    auto preds = node2->getPredecessors();
    TEST_ASSERT_EQUAL(1, static_cast<int>(preds.size()));
    TEST_ASSERT_TRUE(preds[0] == "node1");

    auto succs = node2->getSuccessors();
    TEST_ASSERT_EQUAL(1, static_cast<int>(succs.size()));
    TEST_ASSERT_TRUE(succs[0] == "node3");
}

void testWorkflowNode_Attributes() {
    auto node = std::make_shared<WorkflowNode>("node1", "TestNode", NodeType::TASK);

    node->setAttribute(std::string("key1"), std::string("value1"));
    node->setAttribute(std::string("key2"), std::string("value2"));

    TEST_ASSERT_TRUE(node->getAttribute(std::string("key1"), std::string("")) == "value1");
    TEST_ASSERT_TRUE(node->getAttribute(std::string("key2"), std::string("")) == "value2");
    TEST_ASSERT_TRUE(node->getAttribute(std::string("nonexistent"), std::string("default")) == "default");

    auto attrs = node->getAttributes();
    TEST_ASSERT_TRUE(attrs.size() >= 2);
}

// ========== WorkflowEdge 测试 ==========
void testWorkflowEdge_Create() {
    WorkflowEdge edge(std::string("node1"), std::string("node2"));
    TEST_ASSERT_TRUE(edge.getSourceId() == "node1");
    TEST_ASSERT_TRUE(edge.getTargetId() == "node2");
}

void testWorkflowEdge_Condition() {
    WorkflowEdge edge(std::string("node1"), std::string("node2"));
    TEST_ASSERT_TRUE(edge.getCondition().empty());

    edge.setCondition(std::string("success == true"));
    TEST_ASSERT_TRUE(edge.getCondition() == "success == true");
}

void testWorkflowEdge_Weight() {
    WorkflowEdge edge(std::string("node1"), std::string("node2"), std::string(""));
    TEST_ASSERT_EQUAL(1, edge.getWeight());  // 默认权重是1

    edge.setWeight(10);
    TEST_ASSERT_EQUAL(10, edge.getWeight());
}

// ========== WorkflowGraph 测试 ==========
void testWorkflowGraph_AddAndRemoveNodes() {
    WorkflowGraph graph;

    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);

    graph.addNode(node1);
    graph.addNode(node2);

    TEST_ASSERT_EQUAL(2, static_cast<int>(graph.getNodeCount()));

    TEST_ASSERT_TRUE(graph.removeNode(std::string("node1")));
    TEST_ASSERT_EQUAL(1, static_cast<int>(graph.getNodeCount()));
    TEST_ASSERT_FALSE(graph.removeNode(std::string("nonexistent")));
}

void testWorkflowGraph_AddAndRemoveEdges() {
    WorkflowGraph graph;

    // 先添加节点
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));

    TEST_ASSERT_EQUAL(2, static_cast<int>(graph.getEdgeCount()));

    TEST_ASSERT_TRUE(graph.removeEdge(std::string("node1"), std::string("node2")));
    TEST_ASSERT_EQUAL(1, static_cast<int>(graph.getEdgeCount()));
    TEST_ASSERT_FALSE(graph.removeEdge(std::string("nonexistent1"), std::string("nonexistent2")));
}

void testWorkflowGraph_GetNode() {
    WorkflowGraph graph;
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);

    graph.addNode(node1);

    auto retrieved = graph.getNode(std::string("node1"));
    TEST_ASSERT_NOT_NULL(retrieved.get());
    TEST_ASSERT_TRUE(retrieved->getId() == "node1");

    auto notFound = graph.getNode(std::string("nonexistent"));
    TEST_ASSERT_NULL(notFound.get());
}

void testWorkflowGraph_GetAllNodes() {
    WorkflowGraph graph;

    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);

    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    auto nodes = graph.getAllNodes();
    TEST_ASSERT_EQUAL(3, static_cast<int>(nodes.size()));
}

void testWorkflowGraph_GetAllEdges() {
    WorkflowGraph graph;

    // 先添加节点
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    auto node4 = std::make_shared<WorkflowNode>("node4", "Node4", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);
    graph.addNode(node4);

    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));
    graph.addEdge(std::string("node3"), std::string("node4"), std::string(""));

    auto edges = graph.getAllEdges();
    TEST_ASSERT_EQUAL(3, static_cast<int>(edges.size()));
}

void testWorkflowGraph_TopologicalSort() {
    WorkflowGraph graph;

    // 创建DAG: node1 -> node2 -> node3
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));

    auto sorted = graph.topologicalSort();
    TEST_ASSERT_EQUAL(3, static_cast<int>(sorted.size()));

    // 验证拓扑顺序：node1应该在node2之前，node2应该在node3之前
    bool node1BeforeNode2 = false;
    bool node2BeforeNode3 = false;

    for (size_t i = 0; i < sorted.size(); i++) {
        if (sorted[i] == "node1") {
            for (size_t j = i + 1; j < sorted.size(); j++) {
                if (sorted[j] == "node2") node1BeforeNode2 = true;
            }
        }
        if (sorted[i] == "node2") {
            for (size_t j = i + 1; j < sorted.size(); j++) {
                if (sorted[j] == "node3") node2BeforeNode3 = true;
            }
        }
    }

    TEST_ASSERT_TRUE(node1BeforeNode2);
    TEST_ASSERT_TRUE(node2BeforeNode3);
}

void testWorkflowGraph_IsDAG() {
    WorkflowGraph dagGraph;
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    dagGraph.addNode(node1);
    dagGraph.addNode(node2);
    dagGraph.addNode(node3);

    dagGraph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    dagGraph.addEdge(std::string("node2"), std::string("node3"), std::string(""));
    TEST_ASSERT_TRUE(dagGraph.isDAG());

    WorkflowGraph cyclicGraph;
    auto node4 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node5 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node6 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    cyclicGraph.addNode(node4);
    cyclicGraph.addNode(node5);
    cyclicGraph.addNode(node6);

    cyclicGraph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    cyclicGraph.addEdge(std::string("node2"), std::string("node3"), std::string(""));
    cyclicGraph.addEdge(std::string("node3"), std::string("node1"), std::string("")); // 创建环
    TEST_ASSERT_FALSE(cyclicGraph.isDAG());
}

void testWorkflowGraph_DetectCycles() {
    WorkflowGraph graph;
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    // 创建环: node1 -> node2 -> node3 -> node1
    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));
    graph.addEdge(std::string("node3"), std::string("node1"), std::string(""));

    auto cycles = graph.detectCycles();
    TEST_ASSERT_TRUE(cycles.size() > 0);
}

void testWorkflowGraph_GetStartNodes() {
    WorkflowGraph graph;
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));

    auto startNodes = graph.getStartNodes();
    TEST_ASSERT_EQUAL(1, static_cast<int>(startNodes.size()));
    TEST_ASSERT_TRUE(startNodes[0]->getId() == "node1");
}

void testWorkflowGraph_GetEndNodes() {
    WorkflowGraph graph;
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));

    auto endNodes = graph.getEndNodes();
    TEST_ASSERT_EQUAL(1, static_cast<int>(endNodes.size()));
    TEST_ASSERT_TRUE(endNodes[0]->getId() == "node3");
}

void testWorkflowGraph_Clear() {
    WorkflowGraph graph;
    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1", NodeType::TASK);
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2", NodeType::TASK);
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3", NodeType::TASK);
    graph.addNode(node1);
    graph.addNode(node2);
    graph.addNode(node3);

    graph.addEdge(std::string("node1"), std::string("node2"), std::string(""));
    graph.addEdge(std::string("node2"), std::string("node3"), std::string(""));

    TEST_ASSERT_EQUAL(2, static_cast<int>(graph.getEdgeCount()));

    graph.clear();

    TEST_ASSERT_EQUAL(0, static_cast<int>(graph.getNodeCount()));
    TEST_ASSERT_EQUAL(0, static_cast<int>(graph.getEdgeCount()));
}

void testWorkflowGraph_Name() {
    WorkflowGraph graph;
    graph.setName(std::string("TestGraph"));
    TEST_ASSERT_TRUE(graph.getName() == "TestGraph");
}

// ========== Test Suite 定义 ==========
TestSuite createWorkflowGraphTestSuite() {
    TestSuite suite("工作流图测试");

    // WorkflowNode 测试
    suite.addTest("WorkflowNode_CreateAndGetId", testWorkflowNode_CreateAndGetId);
    suite.addTest("WorkflowNode_SetAndGetState", testWorkflowNode_SetAndGetState);
    suite.addTest("WorkflowNode_PredecessorsAndSuccessors", testWorkflowNode_PredecessorsAndSuccessors);
    suite.addTest("WorkflowNode_Attributes", testWorkflowNode_Attributes);

    // WorkflowEdge 测试
    suite.addTest("WorkflowEdge_Create", testWorkflowEdge_Create);
    suite.addTest("WorkflowEdge_Condition", testWorkflowEdge_Condition);
    suite.addTest("WorkflowEdge_Weight", testWorkflowEdge_Weight);

    // WorkflowGraph 测试
    suite.addTest("WorkflowGraph_AddAndRemoveNodes", testWorkflowGraph_AddAndRemoveNodes);
    suite.addTest("WorkflowGraph_AddAndRemoveEdges", testWorkflowGraph_AddAndRemoveEdges);
    suite.addTest("WorkflowGraph_GetNode", testWorkflowGraph_GetNode);
    suite.addTest("WorkflowGraph_GetAllNodes", testWorkflowGraph_GetAllNodes);
    suite.addTest("WorkflowGraph_GetAllEdges", testWorkflowGraph_GetAllEdges);
    suite.addTest("WorkflowGraph_TopologicalSort", testWorkflowGraph_TopologicalSort);
    suite.addTest("WorkflowGraph_IsDAG", testWorkflowGraph_IsDAG);
    suite.addTest("WorkflowGraph_DetectCycles", testWorkflowGraph_DetectCycles);
    suite.addTest("WorkflowGraph_GetStartNodes", testWorkflowGraph_GetStartNodes);
    suite.addTest("WorkflowGraph_GetEndNodes", testWorkflowGraph_GetEndNodes);
    suite.addTest("WorkflowGraph_Clear", testWorkflowGraph_Clear);
    suite.addTest("WorkflowGraph_Name", testWorkflowGraph_Name);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // 禁用日志以获得更清晰的测试输出
    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowGraphTestSuite());

    return TestRunner::runAllSuites(suites);
}
