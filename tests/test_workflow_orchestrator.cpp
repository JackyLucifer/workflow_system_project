/**
 * @file test_workflow_orchestrator.cpp
 * @brief 工作流编排器测试
 */

#include "workflow_system/implementations/workflow_orchestrator.h"
#include "workflow_system/implementations/workflow_graph.h"
#include "test_framework.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助函数 ==========

std::shared_ptr<WorkflowGraph> createSimpleGraph() {
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("test_graph");

    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1");
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2");
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3");

    // 设置更长的超时时间，避免竞态条件
    node1->setAttribute("timeout", "1000");
    node2->setAttribute("timeout", "1000");
    node3->setAttribute("timeout", "1000");

    graph->addNode(node1);
    graph->addNode(node2);
    graph->addNode(node3);

    graph->addEdge("node1", "node2", "");
    graph->addEdge("node2", "node3", "");

    return graph;
}

std::shared_ptr<WorkflowGraph> createParallelGraph() {
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("parallel_graph");

    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1");
    auto node2a = std::make_shared<WorkflowNode>("node2a", "Node2A");
    auto node2b = std::make_shared<WorkflowNode>("node2b", "Node2B");
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3");

    // 设置更长的超时时间，避免竞态条件
    node1->setAttribute("timeout", "1000");
    node2a->setAttribute("timeout", "1000");
    node2b->setAttribute("timeout", "1000");
    node3->setAttribute("timeout", "1000");

    graph->addNode(node1);
    graph->addNode(node2a);
    graph->addNode(node2b);
    graph->addNode(node3);

    graph->addEdge("node1", "node2a", "");
    graph->addEdge("node1", "node2b", "");
    graph->addEdge("node2a", "node3", "");
    graph->addEdge("node2b", "node3", "");

    return graph;
}

// ========== 测试用例 ==========

void testOrchestrator_InitialState() {
    WorkflowOrchestrator orchestrator;

    TEST_ASSERT_TRUE(orchestrator.getState() == OrchestrationState::IDLE);
    TEST_ASSERT_EQUAL(0, orchestrator.getProgress());
    TEST_ASSERT_TRUE(orchestrator.getGraph() == nullptr);
}

void testOrchestrator_SetGraph() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);

    TEST_ASSERT_TRUE(orchestrator.getGraph() != nullptr);
    TEST_ASSERT_STRING_EQUAL("test_graph", orchestrator.getGraph()->getName().c_str());
}

void testOrchestrator_SequentialExecution() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);
    orchestrator.setExecutionStrategy(ExecutionStrategy::SEQUENTIAL);

    bool result = orchestrator.execute();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(orchestrator.getState() == OrchestrationState::COMPLETED);
    TEST_ASSERT_EQUAL(100, orchestrator.getProgress());
}

void testOrchestrator_ParallelExecution() {
    WorkflowOrchestrator orchestrator;
    auto graph = createParallelGraph();

    orchestrator.setGraph(graph);
    orchestrator.setExecutionStrategy(ExecutionStrategy::PARALLEL);

    bool result = orchestrator.execute();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(orchestrator.getState() == OrchestrationState::COMPLETED);
    TEST_ASSERT_EQUAL(100, orchestrator.getProgress());
}

void testOrchestrator_PauseResume() {
    auto graph = createSimpleGraph();

    // 在一个单独的线程中执行
    auto orchestrator = std::unique_ptr<WorkflowOrchestrator>(new WorkflowOrchestrator());
    orchestrator->setGraph(graph);

    std::atomic<bool> threadFinished{false};
    std::thread execThread([&]() {
        orchestrator->execute();
        threadFinished = true;
    });

    // 等待100ms然后暂停（在第1个节点执行过程中）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    orchestrator->pause();

    auto stateBeforeResume = orchestrator->getState();
    if (stateBeforeResume != OrchestrationState::PAUSED && stateBeforeResume != OrchestrationState::COMPLETED) {
        execThread.join();
        throw std::runtime_error("Expected PAUSED or COMPLETED state, got " + std::to_string(static_cast<int>(stateBeforeResume)));
    }

    // 如果暂停成功，恢复执行
    if (stateBeforeResume == OrchestrationState::PAUSED) {
        orchestrator->resume();
    }

    execThread.join();

    if (!threadFinished) {
        throw std::runtime_error("Thread did not finish");
    }
    // orchestrator 在此处被销毁
}

void testOrchestrator_Abort() {
    auto graph = createSimpleGraph();

    // 在一个单独的线程中执行
    auto orchestrator = std::unique_ptr<WorkflowOrchestrator>(new WorkflowOrchestrator());
    orchestrator->setGraph(graph);

    OrchestrationState finalState;
    std::thread execThread([&]() {
        orchestrator->execute();
        finalState = orchestrator->getState();
    });

    // 等待100ms然后中止（在第1个节点执行过程中）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    orchestrator->abort();

    execThread.join();

    // 执行被中止或已完成
    TEST_ASSERT_TRUE(finalState == OrchestrationState::FAILED ||
                    finalState == OrchestrationState::IDLE ||
                    finalState == OrchestrationState::COMPLETED);
    // orchestrator 在此处被销毁
}

void testOrchestrator_NodeExecutionResults() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);
    orchestrator.execute();

    auto results = orchestrator.getAllResults();

    TEST_ASSERT_TRUE(results.size() >= 3);

    // 验证每个节点都成功执行
    for (const auto& result : results) {
        TEST_ASSERT_TRUE(result.success);
    }
}

void testOrchestrator_Variables() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);

    orchestrator.setVariable("key1", "value1");
    orchestrator.setVariable("key2", "value2");

    TEST_ASSERT_STRING_EQUAL("value1", orchestrator.getVariable("key1", "").c_str());
    TEST_ASSERT_STRING_EQUAL("value2", orchestrator.getVariable("key2", "").c_str());
    TEST_ASSERT_STRING_EQUAL("default", orchestrator.getVariable("nonexistent", "default").c_str());
}

void testOrchestrator_ProgressCallback() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);

    int progressValue = -1;
    orchestrator.setProgressCallback([&](int progress) {
        progressValue = progress;
    });

    orchestrator.execute();

    TEST_ASSERT_EQUAL(100, progressValue);
}

void testOrchestrator_CompletionCallback() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);

    bool callbackCalled = false;
    bool successValue = false;
    std::string errorMsg;

    orchestrator.setCompletionCallback([&](bool success, const std::string& error) {
        callbackCalled = true;
        successValue = success;
        errorMsg = error;
    });

    orchestrator.execute();

    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_TRUE(successValue);
    TEST_ASSERT_TRUE(errorMsg.empty());
}

void testOrchestrator_CycleDetection() {
    WorkflowOrchestrator orchestrator;
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("cycle_graph");

    auto node1 = std::make_shared<WorkflowNode>("node1", "Node1");
    auto node2 = std::make_shared<WorkflowNode>("node2", "Node2");
    auto node3 = std::make_shared<WorkflowNode>("node3", "Node3");

    graph->addNode(node1);
    graph->addNode(node2);
    graph->addNode(node3);

    graph->addEdge("node1", "node2", "");
    graph->addEdge("node2", "node3", "");
    graph->addEdge("node3", "node1", "");  // 创建循环

    orchestrator.setGraph(graph);

    bool result = orchestrator.execute();

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(orchestrator.getState() == OrchestrationState::FAILED);
}

void testOrchestrator_Reset() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);
    orchestrator.execute();

    TEST_ASSERT_TRUE(orchestrator.getState() == OrchestrationState::COMPLETED);
    TEST_ASSERT_EQUAL(100, orchestrator.getProgress());

    orchestrator.reset();

    TEST_ASSERT_TRUE(orchestrator.getState() == OrchestrationState::IDLE);
    TEST_ASSERT_EQUAL(0, orchestrator.getProgress());
}

void testOrchestrator_Context() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    orchestrator.setGraph(graph);
    orchestrator.execute();

    auto* context = orchestrator.getContext();

    TEST_ASSERT_TRUE(context != nullptr);
    TEST_ASSERT_TRUE(context->getGraph() != nullptr);
}

// ========== 简单观察者实现 ==========

class TestObserver : public IWorkflowObserver {
private:
    int startedCount_ = 0;
    int finishedCount_ = 0;
    int interruptedCount_ = 0;
    int errorCount_ = 0;
    std::string lastError_;

public:
    void onWorkflowStarted(const std::string& workflowName) override {
        startedCount_++;
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        finishedCount_++;
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        interruptedCount_++;
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        errorCount_++;
        lastError_ = error;
    }

    int getStartedCount() const { return startedCount_; }
    int getFinishedCount() const { return finishedCount_; }
    int getInterruptedCount() const { return interruptedCount_; }
    int getErrorCount() const { return errorCount_; }
    std::string getLastError() const { return lastError_; }
};

void testOrchestrator_Observer() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    auto observer = std::make_shared<TestObserver>();

    orchestrator.setGraph(graph);
    orchestrator.addObserver(observer);

    orchestrator.execute();

    TEST_ASSERT_EQUAL(1, observer->getStartedCount());
    TEST_ASSERT_EQUAL(1, observer->getFinishedCount());
}

void testOrchestrator_RemoveObserver() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    auto observer = std::make_shared<TestObserver>();

    orchestrator.setGraph(graph);
    orchestrator.addObserver(observer);
    orchestrator.removeObserver(observer);

    orchestrator.execute();

    TEST_ASSERT_EQUAL(0, observer->getStartedCount());
}

void testOrchestrator_ClearObservers() {
    WorkflowOrchestrator orchestrator;
    auto graph = createSimpleGraph();

    auto observer1 = std::make_shared<TestObserver>();
    auto observer2 = std::make_shared<TestObserver>();

    orchestrator.setGraph(graph);
    orchestrator.addObserver(observer1);
    orchestrator.addObserver(observer2);
    orchestrator.clearObservers();

    orchestrator.execute();

    TEST_ASSERT_EQUAL(0, observer1->getStartedCount());
    TEST_ASSERT_EQUAL(0, observer2->getStartedCount());
}

void testOrchestrator_ExecutionStrategy() {
    WorkflowOrchestrator orchestrator;

    TEST_ASSERT_TRUE(orchestrator.getExecutionStrategy() == ExecutionStrategy::SEQUENTIAL);

    orchestrator.setExecutionStrategy(ExecutionStrategy::PARALLEL);
    TEST_ASSERT_TRUE(orchestrator.getExecutionStrategy() == ExecutionStrategy::PARALLEL);

    orchestrator.setExecutionStrategy(ExecutionStrategy::SEQUENTIAL);
    TEST_ASSERT_TRUE(orchestrator.getExecutionStrategy() == ExecutionStrategy::SEQUENTIAL);
}

// ========== 测试套件创建 ==========

TestSuite createWorkflowOrchestratorTestSuite() {
    TestSuite suite("工作流编排器测试");

    suite.addTest("Orchestrator_InitialState", testOrchestrator_InitialState);
    suite.addTest("Orchestrator_SetGraph", testOrchestrator_SetGraph);
    suite.addTest("Orchestrator_SequentialExecution", testOrchestrator_SequentialExecution);
    suite.addTest("Orchestrator_ParallelExecution", testOrchestrator_ParallelExecution);
    suite.addTest("Orchestrator_PauseResume", testOrchestrator_PauseResume);
    suite.addTest("Orchestrator_Abort", testOrchestrator_Abort);
    suite.addTest("Orchestrator_NodeExecutionResults", testOrchestrator_NodeExecutionResults);
    suite.addTest("Orchestrator_Variables", testOrchestrator_Variables);
    suite.addTest("Orchestrator_ProgressCallback", testOrchestrator_ProgressCallback);
    suite.addTest("Orchestrator_CompletionCallback", testOrchestrator_CompletionCallback);
    suite.addTest("Orchestrator_CycleDetection", testOrchestrator_CycleDetection);
    suite.addTest("Orchestrator_Reset", testOrchestrator_Reset);
    suite.addTest("Orchestrator_Context", testOrchestrator_Context);
    suite.addTest("Orchestrator_Observer", testOrchestrator_Observer);
    suite.addTest("Orchestrator_RemoveObserver", testOrchestrator_RemoveObserver);
    suite.addTest("Orchestrator_ClearObservers", testOrchestrator_ClearObservers);
    suite.addTest("Orchestrator_ExecutionStrategy", testOrchestrator_ExecutionStrategy);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowOrchestratorTestSuite());

    return TestRunner::runAllSuites(suites);
}
