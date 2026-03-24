/**
 * @file queue_example.cpp
 * @brief 工作流队列示例 - 演示队列、超时和执行结果
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "core/types.h"
#include "core/logger.h"
#include "core/utils.h"
#include "implementations/system_facade.h"
#include "implementations/workflows/base_workflow.h"

using namespace WorkflowSystem;

// ========== 简单计算工作流 ==========

class SimpleCalculationWorkflow : public BaseWorkflow {
public:
    explicit SimpleCalculationWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("SimpleCalculationWorkflow", resourceManager) {}

protected:
    void onStart() override {
        LOG_INFO("[SimpleCalculationWorkflow] Workflow started");
    }

    void onMessage(const IMessage& message) override {
        LOG_INFO("[SimpleCalculationWorkflow] Received: " + message.getTopic());
    }

    void onInterrupt() override {
        LOG_WARNING("[SimpleCalculationWorkflow] Interrupted");
    }

    void onFinalize() override {
        LOG_INFO("[SimpleCalculationWorkflow] Finished");
    }

    Any onExecute() override {
        // 模拟耗时操作
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 执行简单计算
        int result = 0;
        for (int i = 1; i <= 100; ++i) {
            result += i;
        }

        LOG_INFO("[SimpleCalculationWorkflow] Calculation completed: " + std::to_string(result));
        return Any(result);
    }
};

// ========== 长时间操作工作流 ==========

class LongRunningWorkflow : public BaseWorkflow {
public:
    explicit LongRunningWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("LongRunningWorkflow", resourceManager) {}

protected:
    void onStart() override {
        LOG_INFO("[LongRunningWorkflow] Workflow started");
    }

    void onMessage(const IMessage& message) override {
        LOG_INFO("[LongRunningWorkflow] Received: " + message.getTopic());
    }

    void onInterrupt() override {
        LOG_WARNING("[LongRunningWorkflow] Interrupted");
    }

    void onFinalize() override {
        LOG_INFO("[LongRunningWorkflow] Finished");
    }

    Any onExecute() override {
        // 模拟长时间操作
        int result = 0;
        for (int i = 0; i < 50; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            result += i * 2;
            LOG_INFO("[LongRunningWorkflow] Progress: " + std::to_string(i + 1) + "/50");
        }

        LOG_INFO("[LongRunningWorkflow] Long operation completed: " + std::to_string(result));
        return Any(result);
    }
};

// ========== 便捷打印函数 ==========

void printHeader(const std::string& title) {
    std::cout << "========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

void printSection(const std::string& title) {
    std::cout << std::endl;
    std::cout << ">>> " << title << std::endl;
    std::cout << std::endl;
}

int main() {
    printHeader("Workflow System - Queue Demo");

    // ========== Step 1: Initialize System ==========
    printSection("Step 1: Initialize System");
    auto& system = SystemFacade::getInstance();
    system.initialize("/tmp/workflow_queue_demo");
    auto manager = system.getManager();
    std::cout << "  [OK] System initialized" << std::endl;

    // ========== Step 2: Register Workflows ==========
    printSection("Step 2: Register Workflows");
    auto calcWorkflow = std::make_shared<SimpleCalculationWorkflow>(
        manager->getResourceManager()
    );
    auto longWorkflow = std::make_shared<LongRunningWorkflow>(
        manager->getResourceManager()
    );

    manager->registerWorkflow("Calculation", calcWorkflow);
    manager->registerWorkflow("LongRunning", longWorkflow);
    std::cout << "  [OK] Workflows registered" << std::endl;

    // ========== Step 3: Set Timeout ==========
    printSection("Step 3: Set Default Timeout");
    manager->setDefaultTimeout(5000); // 5 seconds
    std::cout << "  Default timeout: 5000 ms (5 seconds)" << std::endl;

    // ========== Step 4: Create Queue Tasks ==========
    printSection("Step 4: Create Queue Tasks");

    // Task 1: Simple calculation with short timeout
    WorkflowTask task1;
    task1.workflowName = "Calculation";
    task1.timeoutMs = 2000; // 2 seconds
    task1.onComplete = [](const Any& result) {
        try {
            int value = result.cast<int>();
            std::cout << "  [Task 1 Completed] Result: " << value << std::endl;
        } catch (...) {
            std::cout << "  [Task 1 Error] Invalid result type" << std::endl;
        }
    };
    task1.onError = [](const std::string& error) {
        std::cout << "  [Task 1 Failed] Error: " << error << std::endl;
    };
    task1.taskId = "task_001";
    std::cout << "  Task 1: Simple calculation (timeout: 2000ms)" << std::endl;

    // Task 2: Long running with short timeout (will timeout)
    WorkflowTask task2;
    task2.workflowName = "LongRunning";
    task2.timeoutMs = 500; // 0.5 seconds (will timeout)
    task2.onComplete = [](const Any& result) {
        try {
            int value = result.cast<int>();
            std::cout << "  [Task 2 Completed] Result: " << value << std::endl;
        } catch (...) {
            std::cout << "  [Task 2 Error] Invalid result type" << std::endl;
        }
    };
    task2.onError = [](const std::string& error) {
        std::cout << "  [Task 2 Failed] Error: " << error << std::endl;
    };
    task2.taskId = "task_002";
    std::cout << "  Task 2: Long running (timeout: 500ms - will timeout)" << std::endl;

    // Task 3: Normal calculation
    WorkflowTask task3;
    task3.workflowName = "Calculation";
    task3.timeoutMs = 0; // Use default timeout
    task3.onComplete = [](const Any& result) {
        try {
            int value = result.cast<int>();
            std::cout << "  [Task 3 Completed] Result: " << value << std::endl;
        } catch (...) {
            std::cout << "  [Task 3 Error] Invalid result type" << std::endl;
        }
    };
    task3.onError = [](const std::string& error) {
        std::cout << "  [Task 3 Failed] Error: " << error << std::endl;
    };
    task3.taskId = "task_003";
    std::cout << "  Task 3: Normal calculation (default timeout)" << std::endl;

    // ========== Step 5: Enqueue Tasks ==========
    printSection("Step 5: Enqueue Tasks");
    std::cout << "Adding tasks to queue..." << std::endl;

    manager->enqueueWorkflow(task1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    manager->enqueueWorkflow(task2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    manager->enqueueWorkflow(task3);

    std::cout << "  [OK] All tasks enqueued" << std::endl;

    // ========== Step 6: Monitor Queue ==========
    printSection("Step 6: Monitor Queue Processing");

    // Wait for queue to process
    while (manager->getQueueSize() > 0 || manager->isQueueProcessing()) {
        std::cout << "Queue size: " << manager->getQueueSize()
                  << ", Processing: " << (manager->isQueueProcessing() ? "YES" : "NO")
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << std::endl;
    std::cout << "  [OK] All tasks processed" << std::endl;

    // ========== Step 7: Get Execution Results ==========
    printSection("Step 7: Get Execution Results");

    auto results = manager->getAllExecutionResults();
    std::cout << "Total executions: " << results.size() << std::endl << std::endl;

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        std::cout << "Execution #" << (i + 1) << ":" << std::endl;
        std::cout << "  Task ID: " << result.taskId << std::endl;
        std::cout << "  Workflow: " << result.workflowName << std::endl;
        std::cout << "  Final State: " << workflowStateToString(result.finalState) << std::endl;
        std::cout << "  Execution Time: " << result.executionTimeMs << " ms" << std::endl;

        if (!result.errorMessage.empty()) {
            std::cout << "  Error: " << result.errorMessage << std::endl;
        } else if (result.result.has_value()) {
            try {
                int value = result.result.cast<int>();
                std::cout << "  Result: " << value << std::endl;
            } catch (...) {
                std::cout << "  Result: (invalid type)" << std::endl;
            }
        } else {
            std::cout << "  Result: (empty)" << std::endl;
        }
        std::cout << std::endl;
    }

    // ========== Step 8: Display Final Status ==========
    printSection("Step 8: Display Final Status");
    std::cout << "System Status:" << std::endl;
    std::cout << manager->getStatus() << std::endl;

    // ========== Demo Complete ==========
    printHeader("Demo Complete");

    std::cout << "Features Demonstrated:" << std::endl;
    std::cout << "- Workflow Queue Management" << std::endl;
    std::cout << "- Task Timeout Control" << std::endl;
    std::cout << "- Execution Result Tracking" << std::endl;
    std::cout << "- Callback Functions (onComplete, onError)" << std::endl;
    std::cout << std::endl;

    // ========== Cleanup ==========
    std::cout << "Shutting down system..." << std::endl;
    system.shutdown();
    std::cout << "  [OK] System shutdown" << std::endl;

    return 0;
}
