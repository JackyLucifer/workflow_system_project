/**
 * @file comprehensive_example.cpp
 * @brief 工作流系统完整综合示例
 *
 * 本程序演示所有核心模块的协同工作：
 * - 工作流图
 * - 工作流编排器
 * - 观察者模式
 * - 指标收集器
 * - 重试策略
 * - 超时处理器
 * - 资源管理器
 * - 配置管理器
 * - 工作流管理器
 */

#include "workflow_system/interfaces/workflow_graph.h"
#include "workflow_system/interfaces/workflow_orchestrator.h"
#include "workflow_system/interfaces/workflow_observer.h"
#include "workflow_system/interfaces/metrics_collector.h"
#include "workflow_system/interfaces/retry_policy.h"
#include "workflow_system/interfaces/timeout_handler.h"
#include "workflow_system/interfaces/resource_manager.h"
#include "workflow_system/interfaces/workflow_manager.h"

#include "workflow_system/implementations/workflow_graph.h"
#include "workflow_system/implementations/workflow_orchestrator.h"
#include "workflow_system/implementations/workflow_observer_impl.h"
#include "workflow_system/implementations/metrics_collector.h"
#include "workflow_system/implementations/retry_policy.h"
#include <fstream>
#include "workflow_system/implementations/timeout_handler.h"
#include "workflow_system/implementations/filesystem_resource_manager.h"
#include "workflow_system/implementations/workflow_manager.h"
#include "workflow_system/implementations/system_facade.h"

#include "workflow_system/implementations/json_config_manager.h"

#include "workflow_system/core/logger.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

/**
 * @brief 自定义节点执行器（支持重试和超时）
 */
class AdvancedNodeExecutor : public INodeExecutor {
public:
    explicit AdvancedNodeExecutor(std::shared_ptr<IRetryPolicy> retryPolicy = nullptr)
        : retryPolicy_(retryPolicy) {}

    NodeExecutionResult execute(
        const std::shared_ptr<IWorkflowNode>& node,
        IOrchestrationContext* context) override {

        auto startTime = std::chrono::steady_clock::now();

        NodeExecutionResult result;
        result.nodeId = node->getId();
        result.executionTimeMs = 0;
        result.success = false;
        result.errorMessage = "";

        LOG_INFO("[AdvancedExecutor] Executing node: " + node->getName());

        node->setState(NodeState::RUNNING);

        // 获取节点配置
        auto timeout = node->getAttribute("timeout", "500");
        auto failMode = node->getAttribute("failMode", "false");
        auto timeoutBeforeSuccess = node->getAttribute("timeoutBeforeSuccess", "false");
        auto simulateNetworkError = node->getAttribute("simulateNetworkError", "false");

        int timeoutMs = std::stoi(timeout);
        int currentAttempt = 0;
        int maxRetries = retryPolicy_ ? retryPolicy_->getMaxRetries() : 1;

        // 执行节点（支持重试）
        bool success = false;
        while (currentAttempt < maxRetries && !success) {
            currentAttempt++;

            try {
                // 模拟执行时间
                int actualTimeout = timeoutMs;
                if (timeoutBeforeSuccess == "true" && currentAttempt < maxRetries) {
                    actualTimeout = 200; // 前几次快速失败
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(actualTimeout));

                // 检查失败模式
                if (failMode == "true") {
                    throw std::runtime_error("Simulated node failure");
                }

                // 检查网络错误模拟
                if (simulateNetworkError == "true" && currentAttempt < maxRetries) {
                    throw std::runtime_error("Simulated network error");
                }

                // 成功执行
                result.success = true;
                result.errorMessage = "";
                success = true;

                // 添加输出数据
                result.output["status"] = "completed";
                result.output["node"] = node->getName();
                result.output["attempt"] = std::to_string(currentAttempt);
                result.output["timestamp"] = std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());

                node->setState(NodeState::COMPLETED);

            } catch (const std::exception& e) {
                std::string error = std::string("Attempt ") +
                                 std::to_string(currentAttempt) + ": " + e.what();

                LOG_WARNING("[AdvancedExecutor] " + error);

                // 如果有重试策略，决定是否重试
                if (retryPolicy_) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime).count();

                    if (!retryPolicy_->shouldRetry(currentAttempt, error, elapsed)) {
                        break; // 不再重试
                    }

                    // 重试
                    LOG_INFO("[AdvancedExecutor] Retrying node...");
                    continue;
                } else {
                    // 没有重试策略，直接失败
                    break;
                }
            }
        }

        if (!success) {
            result.success = false;
            result.errorMessage = "Max retries exceeded";
            node->setState(NodeState::FAILED);
            LOG_ERROR("[AdvancedExecutor] Node failed: " + result.errorMessage);
        }

        auto endTime = std::chrono::steady_clock::now();
        result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

        return result;
    }

private:
    std::shared_ptr<IRetryPolicy> retryPolicy_;
};

/**
 * @brief 创建带有重试配置的工作流图
 */
std::shared_ptr<IWorkflowGraph> createRetryWorkflowGraph(const std::string& name) {
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName(name);

    // 创建节点
    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "下载配置", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "处理数据", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "验证结果", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    // 配置节点属性
    task1->setAttribute("timeout", "300");
    task1->setAttribute("simulateNetworkError", "true"); // 模拟网络错误

    task2->setAttribute("timeout", "400");
    task2->setAttribute("failMode", "false");

    task3->setAttribute("timeout", "200");
    task3->setAttribute("failMode", "false");

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
 * @brief 示例1: 基础工作流 + 观察者 + 指标收集
 */
void example1_basicWorkflow() {
    std::cout << "\n========== 示例1: 基础工作流 + 观察者 + 指标 ==========\n" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("基础工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "初始化", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "处理", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "结束", NodeType::END);

    task1->setAttribute("timeout", "150");
    task2->setAttribute("timeout", "200");

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "end", "");

    // 创建编排器
    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->setExecutionStrategy(ExecutionStrategy::SEQUENTIAL);

    // 添加观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    auto statsObserver = std::make_shared<StatisticsObserver>();

    orchestrator->addObserver(consoleObserver);
    orchestrator->addObserver(statsObserver);

    // 添加指标收集
    MetricsCollector::getInstance().recordWorkflowStart("基础工作流");

    // 执行工作流
    orchestrator->execute();

    // 记录指标
    MetricsCollector::getInstance().recordWorkflowEnd("基础工作流", true, "");

    // 打印统计
    std::cout << "\n观察者统计报告:" << std::endl;
    statsObserver->printReport();

    // 打印指标
    std::cout << "\n指标收集器报告:" << std::endl;
    std::cout << MetricsCollector::getInstance().generateReport(MetricsReportType::SUMMARY);
}

/**
 * @brief 示例2: 重试策略 + 错误处理
 */
void example2_retryPolicy() {
    std::cout << "\n========== 示例2: 重试策略 + 错误处理 ==========\n" << std::endl;

    // 创建重试策略
    auto retryPolicy = RetryPolicyFactory::createExponentialBackoff(
        3,              // max retries
        500,            // initial delay
        2.0,             // multiplier
        5000);           // max delay

    // 创建带重试的节点执行器
    auto executor = std::make_shared<AdvancedNodeExecutor>(retryPolicy);

    // 创建工作流
    auto graph = createRetryWorkflowGraph("重试工作流");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->setNodeExecutor(executor);
    orchestrator->setExecutionStrategy(ExecutionStrategy::SEQUENTIAL);

    // 添加观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    auto statsObserver = std::make_shared<StatisticsObserver>();

    orchestrator->addObserver(consoleObserver);
    orchestrator->addObserver(statsObserver);

    // 记录指标
    MetricsCollector::getInstance().recordWorkflowStart("重试工作流");

    // 执行工作流
    orchestrator->execute();

    // 记录指标
    MetricsCollector::getInstance().recordWorkflowEnd("重试工作流", true, "");

    std::cout << "\n统计报告:" << std::endl;
    statsObserver->printReport();
}

/**
 * @brief 示例3: 并行执行 + 进度回调
 */
void example3_parallelWithProgress() {
    std::cout << "\n========== 示例3: 并行执行 + 进度回调 ==========\n" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("并行工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto taskA = std::make_shared<WorkflowNode>("taskA", "并行任务A", NodeType::TASK);
    auto taskB = std::make_shared<WorkflowNode>("taskB", "并行任务B", NodeType::TASK);
    auto taskC = std::make_shared<WorkflowNode>("taskC", "并行任务C", NodeType::TASK);
    auto taskD = std::make_shared<WorkflowNode>("taskD", "合并结果", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    taskA->setAttribute("timeout", "300");
    taskB->setAttribute("timeout", "400");
    taskC->setAttribute("timeout", "350");
    taskD->setAttribute("timeout", "200");

    graph->addNode(startNode);
    graph->addNode(taskA);
    graph->addNode(taskB);
    graph->addNode(taskC);
    graph->addNode(taskD);
    graph->addNode(endNode);

    graph->addEdge("start", "taskA", "");
    graph->addEdge("start", "taskB", "");
    graph->addEdge("start", "taskC", "");
    graph->addEdge("taskA", "taskD", "");
    graph->addEdge("taskB", "taskD", "");
    graph->addEdge("taskC", "taskD", "");
    graph->addEdge("taskD", "end", "");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->setExecutionStrategy(ExecutionStrategy::PARALLEL);

    // 设置进度回调
    orchestrator->setProgressCallback([](int progress) {
        std::cout << "[进度] " << progress << "%" << std::endl;
    });

    // 设置完成回调
    orchestrator->setCompletionCallback([](bool success, const std::string& message) {
        std::cout << "\n[完成回调] 工作流" << (success ? "成功" : "失败") << std::endl;
        if (!success) {
            std::cout << "错误信息: " << message << std::endl;
        }
    });

    // 添加观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    orchestrator->addObserver(consoleObserver);

    // 记录指标
    MetricsCollector::getInstance().recordWorkflowStart("并行工作流");

    orchestrator->execute();

    MetricsCollector::getInstance().recordWorkflowEnd("并行工作流", true, "");
}

/**
 * @brief 示例4: 多观察者 + 统计分析
 */
void example4_multipleObservers() {
    std::cout << "\n========== 示例4: 多观察者 + 统计分析 ==========\n" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("多观察者工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "任务1", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "任务2", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    task1->setAttribute("timeout", "100");
    task2->setAttribute("timeout", "100");

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("start", "task2", "");
    graph->addEdge("task1", "end", "");
    graph->addEdge("task2", "end", "");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    // 添加多个观察者
    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    auto loggingObserver = std::make_shared<LoggingObserver>(true);
    auto statsObserver = std::make_shared<StatisticsObserver>();

    orchestrator->addObserver(consoleObserver);
    orchestrator->addObserver(loggingObserver);
    orchestrator->addObserver(statsObserver);

    // 自定义回调观察者
    auto callbackObserver = std::make_shared<CallbackObserver>(
        [](const std::string& name) {
            std::cout << "[自定义观察者] 工作流 '" << name << "' 已开始" << std::endl;
        },
        [](const std::string& name) {
            std::cout << "[自定义观察者] 工作流 '" << name << "' 已完成" << std::endl;
        },
        [](const std::string& name) {
            std::cout << "[自定义观察者] 工作流 '" << name << "' 已中断" << std::endl;
        },
        [](const std::string& name, const std::string& error) {
            std::cout << "[自定义观察者] 工作流 '" << name << "' 错误: " << error << std::endl;
        }
    );

    orchestrator->addObserver(callbackObserver);

    // 记录指标
    MetricsCollector::getInstance().recordWorkflowStart("多观察者工作流");

    orchestrator->execute();

    MetricsCollector::getInstance().recordWorkflowEnd("多观察者工作流", true, "");

    std::cout << "\n统计报告:" << std::endl;
    statsObserver->printReport();
}

/**
 * @brief 示例5: 错误处理和容错
 */
void example5_errorHandling() {
    std::cout << "\n========== 示例5: 错误处理和容错 ==========\n" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("容错工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "可能失败的任务", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "继续任务", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    // task1 可能失败，但设置为不中断工作流
    task1->setAttribute("timeout", "200");
    task1->setAttribute("failMode", "true");
    task1->setAttribute("failOnError", "false");  // 失败不中断

    task2->setAttribute("timeout", "100");

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "end", "");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    auto statsObserver = std::make_shared<StatisticsObserver>();

    orchestrator->addObserver(consoleObserver);
    orchestrator->addObserver(statsObserver);

    MetricsCollector::getInstance().recordWorkflowStart("容错工作流");

    orchestrator->execute();

    MetricsCollector::getInstance().recordWorkflowEnd("容错工作流", false, "Task 1 failed");

    std::cout << "\n统计报告:" << std::endl;
    statsObserver->printReport();
}

/**
 * @brief 示例6: 暂停和恢复
 */
void example6_pauseResume() {
    std::cout << "\n========== 示例6: 暂停和恢复 ==========\n" << std::endl;

    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("暂停恢复工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "长任务1", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "长任务2", NodeType::TASK);
    auto task3 = std::make_shared<WorkflowNode>("task3", "长任务3", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    task1->setAttribute("timeout", "800");
    task2->setAttribute("timeout", "800");
    task3->setAttribute("timeout", "800");

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(task3);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "task3", "");
    graph->addEdge("task3", "end", "");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);
    orchestrator->setExecutionStrategy(ExecutionStrategy::SEQUENTIAL);

    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    orchestrator->addObserver(consoleObserver);

    // 在另一个线程中运行工作流
    std::thread workflowThread([&]() {
        orchestrator->execute();
    });

    // 等待一小段时间后暂停
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\n[主线程] 暂停工作流..." << std::endl;
    orchestrator->pause();

    std::cout << "[主线程] 等待2秒..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[主线程] 恢复工作流..." << std::endl;
    orchestrator->resume();

    // 等待工作流完成
    workflowThread.join();

    std::cout << "\n工作流已结束" << std::endl;
}

/**
 * @brief 示例7: 导出指标报告
 */
void example7_exportMetrics() {
    std::cout << "\n========== 示例7: 导出指标报告 ==========\n" << std::endl;

    // 运行几个工作流来收集数据
    for (int i = 1; i <= 3; ++i) {
        auto graph = std::make_shared<WorkflowGraph>();
        graph->setName("导出测试工作流" + std::to_string(i));

        auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
        auto task1 = std::make_shared<WorkflowNode>("task1", "任务" + std::to_string(i), NodeType::TASK);
        auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

        task1->setAttribute("timeout", "100");

        graph->addNode(startNode);
        graph->addNode(task1);
        graph->addNode(endNode);

        graph->addEdge("start", "task1", "");
        graph->addEdge("task1", "end", "");

        auto orchestrator = std::make_shared<WorkflowOrchestrator>();
        orchestrator->setGraph(graph);

        auto consoleObserver = std::make_shared<ConsoleObserver>(true);
        orchestrator->addObserver(consoleObserver);

        MetricsCollector::getInstance().recordWorkflowStart("导出测试工作流" + std::to_string(i));

        orchestrator->execute();

        MetricsCollector::getInstance().recordWorkflowEnd(
            "导出测试工作流" + std::to_string(i), true, "");

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 导出不同格式的报告
    std::cout << "\n--- 导出 JSON 格式 ---" << std::endl;
    std::string jsonReport = MetricsCollector::getInstance().generateReport(MetricsReportType::JSON);
    std::cout << jsonReport << std::endl;

    std::cout << "\n--- 导出 CSV 格式 ---" << std::endl;
    std::string csvReport = MetricsCollector::getInstance().generateReport(MetricsReportType::CSV);
    std::cout << csvReport << std::endl;

    std::cout << "\n--- 导出详细报告 ---" << std::endl;
    std::string detailedReport = MetricsCollector::getInstance().generateReport(MetricsReportType::DETAILED);
    std::cout << detailedReport << std::endl;

    // 导出到文件
    std::string jsonPath = "/tmp/workflow_metrics.json";
    std::string csvPath = "/tmp/workflow_metrics.csv";

    if (MetricsCollector::getInstance().exportMetrics(jsonPath, MetricsReportType::JSON)) {
        std::cout << "JSON 报告已导出到: " << jsonPath << std::endl;
    }

    if (MetricsCollector::getInstance().exportMetrics(csvPath, MetricsReportType::CSV)) {
        std::cout << "CSV 报告已导出到: " << csvPath << std::endl;
    }
}

/**
 * @brief 示例8: 配置驱动的复杂工作流
 */
void example8_configDriven() {
    std::cout << "\n========== 示例8: 配置驱动的复杂工作流 ==========\n" << std::endl;

    // 使用 JSON 配置管理器
    auto configManager = std::make_shared<JsonConfigManager>();

    // 创建临时配置文件
    std::string configPath = "/tmp/workflow_config.json";
    std::ofstream configFile(configPath);
    configFile << "{\n";
    configFile << "  \"workflows\": [\n";
    configFile << "    {\n";
    configFile << "      \"name\": \"配置工作流\",\n";
    configFile << "      \"timeout\": 150,\n";
    configFile << "      \"parallel\": false,\n";
    configFile << "      \"retryPolicy\": {\n";
    configFile << "        \"maxRetries\": 3,\n";
    configFile << "        \"strategy\": \"exponential_backoff\",\n";
    configFile << "        \"initialDelay\": 500\n";
    configFile << "      }\n";
    configFile << "    }\n";
    configFile << "  ]\n";
    configFile << "}\n";
    configFile.close();

    // 加载配置
    if (configManager->load(configPath)) {
        std::cout << "配置已加载" << std::endl;
    }

    // 从配置创建工作流
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("配置驱动工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "配置任务1", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "配置任务2", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    int timeout = configManager->getInt("workflows[0].timeout", 200);
    task1->setAttribute("timeout", std::to_string(timeout));
    task2->setAttribute("timeout", std::to_string(timeout));

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "end", "");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    bool parallel = configManager->getBool("workflows[0].parallel", false);
    orchestrator->setExecutionStrategy(parallel ? ExecutionStrategy::PARALLEL : ExecutionStrategy::SEQUENTIAL);

    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    orchestrator->addObserver(consoleObserver);

    MetricsCollector::getInstance().recordWorkflowStart("配置驱动工作流");

    orchestrator->execute();

    MetricsCollector::getInstance().recordWorkflowEnd("配置驱动工作流", true, "");
}

/**
 * @brief 示例9: 资源管理集成
 */
void example9_resourceManagement() {
    std::cout << "\n========== 示例9: 资源管理集成 ==========\n" << std::endl;

    // 创建资源管理器
    auto resourceManager = std::make_shared<FileSystemResourceManager>("/tmp/workflow_demo_resources");

    // 创建临时资源
    auto tempDir = resourceManager->createDirectory();
    std::cout << "创建临时资源目录: " << tempDir->getPath() << std::endl;

    // 使用资源创建工作流
    auto graph = std::make_shared<WorkflowGraph>();
    graph->setName("资源管理工作流");

    auto startNode = std::make_shared<WorkflowNode>("start", "开始", NodeType::START);
    auto task1 = std::make_shared<WorkflowNode>("task1", "使用资源", NodeType::TASK);
    auto task2 = std::make_shared<WorkflowNode>("task2", "清理资源", NodeType::TASK);
    auto endNode = std::make_shared<WorkflowNode>("end", "完成", NodeType::END);

    // 将资源ID作为节点属性
    task1->setAttribute("resourceId", tempDir->getId());

    task1->setAttribute("timeout", "150");
    task2->setAttribute("timeout", "100");

    graph->addNode(startNode);
    graph->addNode(task1);
    graph->addNode(task2);
    graph->addNode(endNode);

    graph->addEdge("start", "task1", "");
    graph->addEdge("task1", "task2", "");
    graph->addEdge("task2", "end", "");

    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    orchestrator->setGraph(graph);

    auto consoleObserver = std::make_shared<ConsoleObserver>(true);
    orchestrator->addObserver(consoleObserver);

    orchestrator->execute();

    // 清理资源
    std::cout << "清理资源: " << tempDir->getId() << std::endl;
    resourceManager->cleanup(tempDir->getId());

    size_t cleanedCount = resourceManager->cleanupAll();
    std::cout << "已清理 " << cleanedCount << " 个资源" << std::endl;
}

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  工作流系统完整综合示例" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n本程序演示所有核心模块的协同工作：" << std::endl;
    std::cout << "• 工作流图" << std::endl;
    std::cout << "• 工作流编排器" << std::endl;
    std::cout << "• 观察者模式" << std::endl;
    std::cout << "• 指标收集器" << std::endl;
    std::cout << "• 重试策略" << std::endl;
    std::cout << "• 资源管理器" << std::endl;
    std::cout << "• 配置管理器" << std::endl;
    std::cout << "\n========================================\n" << std::endl;

    try {
        example1_basicWorkflow();
        example2_retryPolicy();
        example3_parallelWithProgress();
        example4_multipleObservers();
        example5_errorHandling();
        example6_pauseResume();
        example7_exportMetrics();
        example8_configDriven();
        example9_resourceManagement();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  所有示例执行完成！" << std::endl;
        std::cout << "========================================" << std::endl;

        // 最终报告
        std::cout << "\n【最终统计报告】" << std::endl;
        std::cout << MetricsCollector::getInstance().generateReport(MetricsReportType::SUMMARY);

    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
