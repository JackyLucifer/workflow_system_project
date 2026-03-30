/**
 * @file observer_example.cpp
 * @brief 工作流观察者模式示例程序
 *
 * 本程序演示工作流系统的观察者模式功能，包括：
 * - LoggingObserver: 记录工作流事件到日志
 * - ConsoleObserver: 在控制台以彩色输出显示工作流事件
 * - StatisticsObserver: 收集和统计工作流执行数据
 * - CallbackObserver: 通过回调函数处理工作流事件
 */

#include "workflow_system/interfaces/workflow_graph.h"
#include "workflow_system/interfaces/workflow_orchestrator.h"
#include "workflow_system/implementations/workflow_graph.h"
#include "workflow_system/implementations/workflow_orchestrator.h"
#include "workflow_system/implementations/workflow_observer_impl.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

/**
 * @brief 创建一个简单的线性工作流
 */
std::shared_ptr<IWorkflowGraph> createSimpleWorkflow(const std::string& name) {
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName(name);

    // 创建节点
    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "数据采集", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "数据处理", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "数据验证", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    // 设置节点属性（模拟不同执行时间）
    task1->setAttribute("timeout", "150");
    task2->setAttribute("timeout", "200");
    task3->setAttribute("timeout", "100");

    // 添加节点到图
    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(task3);
    graph->addNode(endNode);

    // 添加边
    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "task3", "");
    graph->addEdge("task3", "end", "");

    return graph;
}

/**
 * @brief 创建一个可能失败的工作流
 */
std::shared_ptr<IWorkflowGraph> createFailingWorkflow(const std::string& name) {
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName(name);

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "正常任务", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "失败任务", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    task1->setAttribute("timeout", "100");
    task2->setAttribute("timeout", "100");
    task2->setAttribute("failMode", "true");  // 设置失败模式
    task2->setAttribute("failOnError", "true"); // 失败时停止

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "end", "");

    return graph;
}

/**
 * @brief 创建一个复杂的工作流（用于演示并行执行）
 */
std::shared_ptr<IWorkflowGraph> createComplexWorkflow(const std::string& name) {
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName(name);

    auto start = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);

    // 并行任务层
    auto taskA = std::make_shared<WorkflowNode>("taskA", "任务A", NodeType::TASK);
    auto taskB = std::make_shared<WorkflowNode>("taskB", "任务B", NodeType::TASK);
    auto taskC = std::make_shared<WorkflowNode>("taskC", "任务C", NodeType::TASK);

    taskA->setAttribute("timeout", "150");
    taskB->setAttribute("timeout", "180");
    taskC->setAttribute("timeout", "120");

    // 第二层任务
    auto taskD = std::make_shared<WorkflowNode>("taskD", "任务D", NodeType::TASK);
    auto taskE = std::make_shared<WorkflowNode>("taskE", "任务E", NodeType::TASK);

    taskD->setAttribute("timeout", "100");
    taskE->setAttribute("timeout", "100");

    // 结束节点
    auto end = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    // 添加节点
    graph->addNode(start);
    graph->addNode(taskA);
    graph->addNode(taskB);
    graph->addNode(taskC);
    graph->addNode(taskD);
    graph->addNode(taskE);
    graph->addNode(end);

    // 添加边（并行结构）
    graph->addEdge("start", "taskA", "");
    graph->addEdge("start", "taskB", "");
    graph->addEdge("start", "taskC", "");

    graph->addEdge("taskA", "taskD", "");
    graph->addEdge("taskB", "taskD", "");
    graph->addEdge("taskC", "taskE", "");

    graph->addEdge("taskD", "end", "");
    graph->addEdge("taskE", "end", "");

    return graph;
}

/**
 * @brief 示例1: 使用 ConsoleObserver 控制台输出
 */
void example1_consoleObserver() {
    std::cout << "\n========== 示例1: ConsoleObserver ==========\n" << std::endl;

    // 创建工作流
    auto graph = createSimpleWorkflow("示例工作流1");

    // 创建编排器
    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 添加 ConsoleObserver
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);  // 启用彩色输出
    orchestrator->addObserver(consoleObserver);

    // 执行工作流
    orchestrator->execute();

    std::cout << std::endl;
}

/**
 * @brief 示例2: 使用 LoggingObserver 日志记录
 */
void example2_loggingObserver() {
    std::cout << "\n========== 示例2: LoggingObserver ==========\n" << std::endl;

    auto graph = createSimpleWorkflow("示例工作流2");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 添加 LoggingObserver
    auto loggingObserver = std::make_shared<LoggingObserver>(true);
    orchestrator->addObserver(loggingObserver);

    // 执行工作流
    orchestrator->execute();

    std::cout << std::endl;
}

/**
 * @brief 示例3: 使用 StatisticsObserver 统计收集
 */
void example3_statisticsObserver() {
    std::cout << "\n========== 示例3: StatisticsObserver ==========\n" << std::endl;

    auto statsObserver = std::make_shared<StatisticsObserver>();

    // 执行多个工作流以收集统计数据
    for (int i = 1; i <= 3; ++i) {
        auto graph = createSimpleWorkflow("统计工作流" + std::to_string(i));

        auto orchestrator = std::make_shared<WorkflowOrchestrator>();
        orchestrator->setGraph(graph);
        orchestrator->addObserver(statsObserver);

        std::cout << "执行第 " << i << " 个工作流..." << std::endl;
        orchestrator->execute();
        std::cout << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 打印统计报告
    std::cout << "\n=== 统计报告 ===" << std::endl;
    statsObserver->printReport();
}

/**
 * @brief 示例4: 使用 CallbackObserver 自定义回调
 */
void example4_callbackObserver() {
    std::cout << "\n========== 示例4: CallbackObserver ==========\n" << std::endl;

    int startedCount = 0;
    int finishedCount = 0;

    // 创建回调观察者
    auto callbackObserver = std::make_shared<CallbackObserver>(
        [&startedCount](const std::string& name) {
            startedCount++;
            std::cout << "[回调] 工作流已启动: " << name << std::endl;
        },
        [&finishedCount](const std::string& name) {
            finishedCount++;
            std::cout << "[回调] 工作流已完成: " << name << std::endl;
        },
        [](const std::string& name) {
            std::cout << "[回调] 工作流已中断: " << name << std::endl;
        },
        [](const std::string& name, const std::string& error) {
            std::cout << "[回调] 工作流错误 [" << name << "]: " << error << std::endl;
        }
    );

    auto graph = createSimpleWorkflow("回调工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->addObserver(callbackObserver);

    orchestrator->execute();

    std::cout << "\n统计: 启动=" << startedCount << ", 完成=" << finishedCount << std::endl;
}

/**
 * @brief 示例5: 组合多个观察者
 */
void example5_multipleObservers() {
    std::cout << "\n========== 示例5: 组合多个观察者 ==========\n" << std::endl;

    auto graph = createSimpleWorkflow("多观察者工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 添加多个观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    auto loggingObserver = std::make_shared<LoggingObserver>(true);
    auto statsObserver = std::make_shared<StatisticsObserver>();

    orchestrator->addObserver(consoleObserver);
    orchestrator->addObserver(loggingObserver);
    orchestrator->addObserver(statsObserver);

    std::cout << "执行工作流，多个观察者将同时接收事件..." << std::endl;
    orchestrator->execute();

    std::cout << "\n观察者统计报告:" << std::endl;
    statsObserver->printReport();
}

/**
 * @brief 示例6: 演示错误处理
 */
void example6_errorHandling() {
    std::cout << "\n========== 示例6: 错误处理 ==========\n" << std::endl;

    auto graph = createFailingWorkflow("失败工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 添加 ConsoleObserver 以显示彩色错误信息
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    orchestrator->addObserver(consoleObserver);

    std::cout << "执行一个会失败的工作流..." << std::endl;
    orchestrator->execute();

    std::cout << std::endl;
}

/**
 * @brief 示例7: 并行执行与观察者
 */
void example7_parallelExecution() {
    std::cout << "\n========== 示例7: 并行执行与观察者 ==========\n" << std::endl;

    auto graph = createComplexWorkflow("并行工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->setExecutionStrategy(ExecutionStrategy::PARALLEL);

    // 添加观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    auto statsObserver = std::make_shared<StatisticsObserver>();

    orchestrator->addObserver(consoleObserver);
    orchestrator->addObserver(statsObserver);

    std::cout << "执行并行工作流..." << std::endl;
    orchestrator->execute();

    std::cout << "\n并行执行统计:" << std::endl;
    statsObserver->printReport();
}

/**
 * @brief 示例8: 动态管理观察者
 */
void example8_dynamicObserverManagement() {
    std::cout << "\n========== 示例8: 动态管理观察者 ==========\n" << std::endl;

    auto observer1 = std::make_shared<ConsoleObserver>(true);
    auto observer2 = std::make_shared<LoggingObserver>(true);
    auto observer3 = std::make_shared<StatisticsObserver>();

    auto graph = createSimpleWorkflow("动态管理工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 第一次执行：只使用 observer1
    std::cout << "\n第一次执行（仅 ConsoleObserver）:" << std::endl;
    orchestrator->addObserver(observer1);
    orchestrator->execute();
    orchestrator->removeObserver(observer1);

    // 第二次执行：使用 observer2 和 observer3
    std::cout << "\n第二次执行（LoggingObserver + StatisticsObserver）:" << std::endl;
    orchestrator->addObserver(observer2);
    orchestrator->addObserver(observer3);
    orchestrator->execute();

    std::cout << "\n统计报告:" << std::endl;
    observer3->printReport();
}

/**
 * @brief 示例9: 使用进度回调和观察者
 */
void example9_progressCallback() {
    std::cout << "\n========== 示例9: 进度回调与观察者 ==========\n" << std::endl;

    auto graph = createComplexWorkflow("进度回调工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->setExecutionStrategy(ExecutionStrategy::PARALLEL);

    // 添加观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    orchestrator->addObserver(consoleObserver);

    // 设置进度回调
    orchestrator->setProgressCallback([](int progress) {
        std::cout << "[进度] " << progress << "%" << std::endl;
    });

    // 设置完成回调
    orchestrator->setCompletionCallback([](bool success, const std::string& message) {
        if (success) {
            std::cout << "\n[完成回调] 工作流成功完成！" << std::endl;
        } else {
            std::cout << "\n[完成回调] 工作流失败: " << message << std::endl;
        }
    });

    orchestrator->execute();
}

/**
 * @brief 示例10: 自定义观察者
 */
class CustomObserver : public IWorkflowObserver {
public:
    void onWorkflowStarted(const std::string& workflowName) override {
        std::cout << ">>> 自定义观察者: 工作流 \"" << workflowName << "\" 已开始 <<<" << std::endl;
        std::cout << "    时间: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        std::cout << ">>> 自定义观察者: 工作流 \"" << workflowName << "\" 已完成 <<<" << std::endl;
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        std::cout << ">>> 自定义观察者: 工作流 \"" << workflowName << "\" 已中断 <<<" << std::endl;
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        std::cout << ">>> 自定义观察者: 工作流 \"" << workflowName << "\" 错误: " << error << " <<<" << std::endl;
    }
};

void example10_customObserver() {
    std::cout << "\n========== 示例10: 自定义观察者 ==========\n" << std::endl;

    auto graph = createSimpleWorkflow("自定义观察者工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 添加自定义观察者
    auto customObserver = std::make_shared<CustomObserver>();
    orchestrator->addObserver(customObserver);

    orchestrator->execute();
}

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  工作流观察者模式示例程序" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        example1_consoleObserver();
        example2_loggingObserver();
        example3_statisticsObserver();
        example4_callbackObserver();
        example5_multipleObservers();
        example6_errorHandling();
        example7_parallelExecution();
        example8_dynamicObserverManagement();
        example9_progressCallback();
        example10_customObserver();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  所有示例执行完成！" << std::endl;
        std::cout << "========================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
