/**
 * @file refactoring_examples.cpp
 * @brief 重构功能使用示例
 *
 * 展示如何使用：
 * - 统一的日志宏
 * - 统一的异常处理
 * - 优化后的组件
 */

#include "workflow_system/core/logging_macros.h"
#include "workflow_system/core/exceptions.h"
#include "workflow_system/implementations/workflow_context.h"
#include "workflow_system/implementations/async_logger.h"
#include "workflow_system/implementations/memory_pool.h"
#include <iostream>
#include <thread>
#include <vector>

using namespace WorkflowSystem;

// ========== 示例1: 日志宏使用 ==========

class MyWorkflow {
public:
    void execute() {
        // 使用日志宏（自动包含类名和方法名）
        LOG_INFO(MyWorkflow, "execute") << "Starting workflow execution";

        try {
            doWork();

            LOG_INFO(MyWorkflow, "execute") << "Workflow completed successfully";
        } catch (const std::exception& e) {
            LOG_ERROR(MyWorkflow, "execute") << "Workflow failed: " << e.what();
            throw;
        }
    }

private:
    void doWork() {
        // 简单日志
        LOG_DEBUG(MyWorkflow, "doWork") << "Processing task";

        // 条件日志
        LOG_DEBUG_IF(true, MyWorkflow, "doWork") << "This will be logged";
        LOG_DEBUG_IF(false, MyWorkflow, "doWork") << "This won't be logged";
    }
};

void demonstrateLoggingMacros() {
    std::cout << "\n========== 日志宏示例 ==========\n" << std::endl;

    // 配置 AsyncLogger
    AsyncLoggerConfig config;
    config.logFilePath = "/tmp/refactoring_example.log";
    config.enableConsole = true;
    config.enableFile = false;  // 示例中不写文件
    AsyncLogger::instance().setConfig(config);

    MyWorkflow workflow;
    workflow.execute();
}

// ========== 示例2: 异常处理使用 ==========

void demonstrateExceptions() {
    std::cout << "\n========== 异常处理示例 ==========\n" << std::endl;

    // 方式1: 基本异常抛出
    TRY_WF("Basic exception") {
        throw WorkflowException("Something went wrong");
    } CATCH_WF;

    // 方式2: 状态异常
    TRY_WF("State transition") {
        throw WorkflowStateException(
            "Invalid state transition",
            WorkflowState::RUNNING,
            WorkflowState::COMPLETED
        );
    } CATCH_WF;

    // 方式3: 带上下文的异常
    TRY_WF_OR_THROW("Contextual exception") {
        WorkflowException e("Operation failed");
        e.addContext("workflow_id", "wf_123");
        e.addContext("node_id", "node_456");
        e.addContext("retry_count", "3");
        throw e;
    } CATCH_WF_OR_THROW;
}

// ========== 示例3: 优化后的 WorkflowContext ==========

void demonstrateWorkflowContext() {
    std::cout << "\n========== WorkflowContext 优化示例 ==========\n" << std::endl;

    auto context = std::make_shared<WorkflowContext>(nullptr);

    // 写操作（独占锁）
    context->set("key1", "value1");
    context->set("key2", "value2");
    context->set("key3", "value3");

    // 读操作（共享锁）- 多个线程可以并发读取
    std::cout << "Single-threaded read: " << context->get("key1") << std::endl;

    // 并发读取测试
    const int numThreads = 4;
    const int readsPerThread = 1000;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&context, readsPerThread]() {
            for (int j = 0; j < readsPerThread; ++j) {
                context->get("key1");
                context->get("key2");
                context->has("key3");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Concurrent read test:" << std::endl;
    std::cout << "  Threads: " << numThreads << std::endl;
    std::cout << "  Reads per thread: " << readsPerThread << std::endl;
    std::cout << "  Total reads: " << (numThreads * readsPerThread * 3) << std::endl;
    std::cout << "  Time: " << duration.count() << "ms" << std::endl;
}

// ========== 示例4: 内存池使用 ==========

struct TestData {
    int id;
    double value;
    char data[128];

    TestData(int i = 0, double v = 0.0) : id(i), value(v) {}
};

void demonstrateMemoryPool() {
    std::cout << "\n========== 内存池示例 ==========\n" << std::endl;

    const int iterations = 10000;

    // 使用内存池
    MemoryPool<TestData, 1024> pool;

    std::vector<TestData*> ptrs;
    ptrs.reserve(iterations);

    // 分配
    for (int i = 0; i < iterations; ++i) {
        TestData* data = pool.allocate();
        new(data) TestData(i, i * 1.5);
        ptrs.push_back(data);
    }

    std::cout << "Allocated " << iterations << " objects" << std::endl;
    std::cout << "Available: " << pool.getAvailableCount() << std::endl;

    // 释放
    for (auto ptr : ptrs) {
        ptr->~TestData();
        pool.deallocate(ptr);
    }

    std::cout << "After deallocation: " << pool.getAvailableCount() << std::endl;

    // 使用对象池（更简单）
    std::cout << "\n使用对象池（自动管理）：" << std::endl;
    ObjectPool<TestData> objPool(10);

    {
        auto obj1 = objPool.acquire();
        auto obj2 = objPool.acquire();
        std::cout << "Pool size: " << objPool.size() << std::endl;
    }  // 自动归还

    std::cout << "After return: " << objPool.size() << std::endl;
}

// ========== 示例5: 异步日志配置 ==========

void demonstrateAsyncLoggerConfig() {
    std::cout << "\n========== AsyncLogger 配置示例 ==========\n" << std::endl;

    // 基础配置
    AsyncLoggerConfig config;
    config.logFilePath = "/tmp/workflow.log";
    config.enableConsole = true;
    config.enableFile = false;  // 示例中不写文件
    config.maxQueueSize = 10000;
    config.flushOnWrite = true;

    // 设置错误回调
    config.errorCallback = [](const std::string& error) {
        std::cerr << "Logger error: " << error << std::endl;
    };

    AsyncLogger::instance().setConfig(config);

    // 测试日志
    AsyncLogger::instance().info("Application started");
    AsyncLogger::instance().warning("This is a warning");
    AsyncLogger::instance().error("This is an error");

    std::cout << "Logger configuration complete" << std::endl;
}

// ========== 主函数 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  重构功能示例程序" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateLoggingMacros();
        demonstrateExceptions();
        demonstrateWorkflowContext();
        demonstrateMemoryPool();
        demonstrateAsyncLoggerConfig();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  所有示例执行完成！" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
