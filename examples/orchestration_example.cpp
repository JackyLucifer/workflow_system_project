/**
 * @file orchestration_example.cpp
 * @brief 工作流编排器使用示例
 *
 * 演示工作流编排的各种功能：
 * - 创建工作流图
 * - 拓扑排序和DAG验证
 * - 顺序执行工作流
 * - 并行执行工作流
 * - 暂停和恢复
 * - 工作流变量和上下文
 * - 条件分支
 */

#include "workflow_system/implementations/workflow_orchestrator.h"
#include "workflow_system/implementations/workflow_graph.h"
#include "workflow_system/implementations/json_config_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

/**
 * @brief 打印节点状态
 */
std::string stateToString(NodeState state) {
    switch (state) {
        case NodeState::PENDING: return "PENDING";
        case NodeState::RUNNING: return "RUNNING";
        case NodeState::COMPLETED: return "COMPLETED";
        case NodeState::FAILED: return "FAILED";
        case NodeState::SKIPPED: return "SKIPPED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 打印工作流图
 */
void printGraph(const std::shared_ptr<IWorkflowGraph>& graph) {
    std::cout << "\n=== 工作流图结构 ===" << std::endl;
    std::cout << "名称: " << graph->getName() << std::endl;
    std::cout << "节点数: " << graph->getNodeCount() << std::endl;
    std::cout << "边数: " << graph->getEdgeCount() << std::endl;
    std::cout << "是否为DAG: " << (graph->isDAG() ? "是" : "否") << std::endl;

    auto startNodes = graph->getStartNodes();
    std::cout << "\n开始节点:" << std::endl;
    for (const auto& node : startNodes) {
        std::cout << "  - " << node->getName() << " (" << node->getId() << ")" << std::endl;
    }

    auto endNodes = graph->getEndNodes();
    std::cout << "\n结束节点:" << std::endl;
    for (const auto& node : endNodes) {
        std::cout << "  - " << node->getName() << " (" << node->getId() << ")" << std::endl;
    }

    std::cout << "\n所有节点:" << std::endl;
    auto allNodes = graph->getAllNodes();
    for (const auto& node : allNodes) {
        std::cout << "  " << node->getName() << " (" << node->getId() << ")" << std::endl;
        std::cout << "    类型: ";
        switch (node->getType()) {
            case NodeType::TASK: std::cout << "TASK"; break;
            case NodeType::CONDITION: std::cout << "CONDITION"; break;
            case NodeType::PARALLEL: std::cout << "PARALLEL"; break;
            case NodeType::START: std::cout << "START"; break;
            case NodeType::END: std::cout << "END"; break;
            case NodeType::MERGE: std::cout << "MERGE"; break;
            default: std::cout << "UNKNOWN";
        }
        std::cout << std::endl;

        auto preds = node->getPredecessors();
        if (!preds.empty()) {
            std::cout << "    前置: ";
            for (const auto& pred : preds) {
                std::cout << pred << " ";
            }
            std::cout << std::endl;
        }

        auto succs = node->getSuccessors();
        if (!succs.empty()) {
            std::cout << "    后置: ";
            for (const auto& succ : succs) {
                std::cout << succ << " ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "====================\n" << std::endl;
}

/**
 * @brief 打印执行结果
 */
void printResults(const std::vector<NodeExecutionResult>& results) {
    std::cout << "\n=== 执行结果 ===" << std::endl;
    for (const auto& result : results) {
        std::cout << "节点: " << result.nodeId << std::endl;
        std::cout << "  成功: " << (result.success ? "是" : "否") << std::endl;
        std::cout << "  执行时间: " << result.executionTimeMs << "ms" << std::endl;
        if (!result.errorMessage.empty()) {
            std::cout << "  错误: " << result.errorMessage << std::endl;
        }
        if (!result.output.empty()) {
            std::cout << "  输出:" << std::endl;
            for (const auto& pair : result.output) {
                std::cout << "    " << pair.first << " = " << pair.second << std::endl;
            }
        }
    }
    std::cout << "==================\n" << std::endl;
}

/**
 * @brief 示例 1: 简单的顺序工作流
 */
void example1_sequentialWorkflow() {
    std::cout << "\n========== 示例 1: 简单的顺序工作流 ==========" << std::endl;

    // 创建工作流图
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("SequentialWorkflow");

    // 创建节点
    auto start = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "数据导入", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "数据处理", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "数据分析", NodeType::TASK);
    auto end = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    // 设置属性
    task1->setAttribute("timeout", "200");
    task2->setAttribute("timeout", "300");
    task3->setAttribute("timeout", "150");

    // 添加节点
    graph->addNode(start);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(task3);
    graph->addNode(end);

    // 添加边
    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "task3", "");
    graph->addEdge("task3", "end", "");

    // 打印图结构
    printGraph(graph);

    // 验证图
    GraphValidator validator;
    auto errors = validator.validate(*graph);
    if (!errors.empty()) {
        std::cout << "\n图验证失败:" << std::endl;
        for (const auto& error : errors) {
            std::cout << "  - " << error.message;
            if (!error.suggestion.empty()) {
                std::cout << " (建议: " << error.suggestion << ")";
            }
            std::cout << std::endl;
        }
        return;
    }

    // 拓扑排序
    auto order = graph->topologicalSort();
    if (!order.empty()) {
        std::cout << "\n拓扑排序:" << std::endl;
        for (const auto& nodeId : order) {
            std::cout << "  " << nodeId << std::endl;
        }
    }

    // 创建编排器并执行
    WorkflowOrchestrator orchestrator;
    orchestrator.setGraph(graph);

    std::cout << "\n开始执行..." << std::endl;
    orchestrator.execute();

    // 打印结果
    auto results = orchestrator.getAllResults();
    printResults(results);
}

/**
 * @brief 示例 2: 并行工作流
 */
void example2_parallelWorkflow() {
    std::cout << "\n========== 示例 2: 并行工作流 ==========" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("ParallelWorkflow");

    // 创建节点
    auto start = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "并行任务1", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "并行任务2", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "并行任务3", NodeType::TASK);
    auto merge = std::make_shared<WorkflowNode>("merge", "合并结果", NodeType::MERGE);
    auto end = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    // 设置属性
    task1->setAttribute("timeout", "300");
    task2->setAttribute("timeout", "250");
    task3->setAttribute("timeout", "350");
    merge->setAttribute("timeout", "100");

    graph->addNode(start);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(task3);
    graph->addNode(merge);
    graph->addNode(end);

    // 构建并行结构
    graph->addEdge("start", "task1", "");
    graph->addEdge("start", "task2", "");
    graph->addEdge("start", "task3", "");
    graph->addEdge("task1", "merge", "");
    graph->addEdge("task2", "merge", "");
    graph->addEdge("task3", "merge", "");
    graph->addEdge("merge", "end", "");

    printGraph(graph);

    // 并行执行
    WorkflowOrchestrator orchestrator;
    orchestrator.setGraph(graph);
    orchestrator.setExecutionStrategy(ExecutionStrategy::PARALLEL);

    std::cout << "\n开始并行执行..." << std::endl;
    orchestrator.execute();

    auto results = orchestrator.getAllResults();
    printResults(results);
}

/**
 * @brief 示例 3: 包含错误处理的工作流
 */
void example3_errorHandling() {
    std::cout << "\n========== 示例 3: 包含错误处理的工作流 ==========" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("ErrorHandlingWorkflow");

    auto start = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "任务1（会失败）", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "任务2", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "错误恢复", NodeType::TASK);
    auto end = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    // 设置属性
    task1->setAttribute("timeout", "100");
    task1->setAttribute("failMode", "true");  // 这个任务会失败
    task1->setAttribute("failOnError", "false");  // 失败后继续执行
    task2->setAttribute("timeout", "100");
    task2->setAttribute("failOnError", "true");   // 失败后停止
    task3->setAttribute("timeout", "100");

    graph->addNode(start);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(task3);
    graph->addNode(end);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task1", "task3", "");  // 失败后的恢复路径
    graph->addEdge("task2", "end", "");
    graph->addEdge("task3", "end", "");

    printGraph(graph);

    WorkflowOrchestrator orchestrator;
    orchestrator.setGraph(graph);

    std::cout << "\n开始执行（包含错误恢复）..." << std::endl;
    orchestrator.execute();

    auto results = orchestrator.getAllResults();
    printResults(results);
}

/**
 * @brief 示例 4: 复杂工作流图
 */
void example4_complexGraph() {
    std::cout << "\n========== 示例 4: 复杂工作流图 ==========" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("ComplexWorkflow");

    // 创建多个分支
    auto start = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);

    // 分支1
    auto branch1_task1 = std::make_shared<WorkflowNode>("b1_t1", "分支1-任务1", NodeType::TASK);
    auto branch1_task2 = std::make_shared<WorkflowNode>("b1_t2", "分支1-任务2", NodeType::TASK);

    // 分支2
    auto branch2_task1 = std::make_shared<WorkflowNode>("b2_t1", "分支2-任务1", NodeType::TASK);
    auto branch2_task2 = std::make_shared<WorkflowNode>("b2_t2", "分支2-任务2", NodeType::TASK);

    // 分支3
    auto branch3_task1 = std::make_shared<WorkflowNode>("b3_t1", "分支3-任务1", NodeType::TASK);

    // 合并
    auto merge = std::make_shared<WorkflowNode>("merge", "合并", NodeType::MERGE);
    auto finalize = std::make_shared<WorkflowNode>("finalize", "最终处理", NodeType::TASK);
    auto end = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    // 设置超时
    branch1_task1->setAttribute("timeout", "150");
    branch1_task2->setAttribute("timeout", "150");
    branch2_task1->setAttribute("timeout", "120");
    branch2_task2->setAttribute("timeout", "180");
    branch3_task1->setAttribute("timeout", "200");
    merge->setAttribute("timeout", "100");
    finalize->setAttribute("timeout", "100");

    graph->addNode(start);
    graph->addNode(branch1_task1);
    graph->addNode(branch1_task2);
    graph->addNode(branch2_task1);
    graph->addNode(branch2_task2);
    graph->addNode(branch3_task1);
    graph->addNode(merge);
    graph->addNode(finalize);
    graph->addNode(end);

    // 构建图结构
    graph->addEdge("start", "b1_t1", "");
    graph->addEdge("start", "b2_t1", "");
    graph->addEdge("start", "b3_t1", "");

    graph->addEdge("b1_t1", "b1_t2", "");
    graph->addEdge("b2_t1", "b2_t2", "");

    graph->addEdge("b1_t2", "merge", "");
    graph->addEdge("b2_t2", "merge", "");
    graph->addEdge("b3_t1", "merge", "");

    graph->addEdge("merge", "finalize", "");
    graph->addEdge("finalize", "end", "");

    printGraph(graph);

    // 检测环（如果图中有环会失败）
    auto cycles = graph->detectCycles();
    if (!cycles.empty()) {
        std::cout << "\n检测到 " << cycles.size() << " 个环:" << std::endl;
        for (size_t i = 0; i < cycles.size(); ++i) {
            std::cout << "  环 " << (i + 1) << ": ";
            for (const auto& nodeId : cycles[i]) {
                std::cout << nodeId << " -> ";
            }
            std::cout << cycles[i][0] << std::endl;
        }
    }

    WorkflowOrchestrator orchestrator;
    orchestrator.setGraph(graph);
    orchestrator.setExecutionStrategy(ExecutionStrategy::PARALLEL);

    std::cout << "\n开始执行复杂工作流..." << std::endl;
    orchestrator.execute();

    auto results = orchestrator.getAllResults();
    printResults(results);
}

/**
 * @brief 示例 5: 使用配置驱动工作流
 */
void example5_configDriven() {
    std::cout << "\n========== 示例 5: 配置驱动的工作流 ==========" << std::endl;

    auto& config = JsonConfigManager::getInstance();

    // 从配置读取工作流参数
    std::string timeout = config.getString("workflow.timeout", "200");
    std::string parallelEnabled = config.getString("workflow.parallel", "true");
    std::string failOnError = config.getString("workflow.failOnError", "false");

    std::cout << "配置参数:" << std::endl;
    std::cout << "  超时: " << timeout << "ms" << std::endl;
    std::cout << "  并行执行: " << (parallelEnabled == "true" ? "是" : "否") << std::endl;
    std::cout << "  遇错停止: " << (failOnError == "true" ? "是" : "否") << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("ConfigDrivenWorkflow");

    auto start = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "配置驱动任务1", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "配置驱动任务2", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "配置驱动任务3", NodeType::TASK);
    auto end = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    // 应用配置
    task1->setAttribute("timeout", timeout);
    task2->setAttribute("timeout", timeout);
    task3->setAttribute("timeout", timeout);
    task1->setAttribute("failOnError", failOnError);
    task2->setAttribute("failOnError", failOnError);
    task3->setAttribute("failOnError", failOnError);

    graph->addNode(start);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(task3);
    graph->addNode(end);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "task3", "");
    graph->addEdge("task3", "end", "");

    WorkflowOrchestrator orchestrator;
    orchestrator.setGraph(graph);
    orchestrator.setExecutionStrategy(
        parallelEnabled == "true" ? ExecutionStrategy::PARALLEL : ExecutionStrategy::SEQUENTIAL
    );

    std::cout << "\n开始执行配置驱动的工作流..." << std::endl;
    orchestrator.execute();

    auto results = orchestrator.getAllResults();
    printResults(results);
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  工作流编排器示例程序" << std::endl;
    std::cout << "==========================================" << std::endl;

    // 运行所有示例
    example1_sequentialWorkflow();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    example2_parallelWorkflow();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    example3_errorHandling();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    example4_complexGraph();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    example5_configDriven();

    std::cout << "\n==========================================" << std::endl;
    std::cout << "  所有示例执行完成！" << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}
