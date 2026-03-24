/**
 * @file advanced_example.cpp
 * @brief 高级功能示例 - 展示超时控制、重试机制和指标收集
 */

#include "implementations/system_facade.h"
#include "implementations/workflows/base_workflow.h"
#include "implementations/timeout_handler.h"
#include "implementations/retry_policy.h"
#include "implementations/metrics_collector.h"
#include "interfaces/workflow_observer.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

/**
 * @brief 简单的消息实现
 */
class SimpleMessage : public IMessage {
public:
    SimpleMessage(const std::string& topic, const std::string& payload)
        : type_("simple"), topic_(topic), payload_(payload) {}

    std::string getType() const override { return type_; }
    std::string getTopic() const override { return topic_; }
    std::string getPayload() const override { return payload_; }

private:
    std::string type_, topic_, payload_;
};

/**
 * @brief 超时工作流示例
 */
class TimeoutWorkflow : public BaseWorkflow {
public:
    TimeoutWorkflow(std::shared_ptr<IResourceManager> resourceManager, int64_t timeoutMs)
        : BaseWorkflow("TimeoutWorkflow", resourceManager), timeoutMs_(timeoutMs) {}

protected:
    void onStart() override {
        LOG_INFO("[TimeoutWorkflow] Workflow started");
        auto ctx = getContext();
        if (ctx) {
            ctx->set("status", "running");
        }
    }

    void onMessage(const IMessage& message) override {
        LOG_INFO("[TimeoutWorkflow] Processing: " + message.getPayload());

        // 模拟长时间处理
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void onInterrupt() override {
        LOG_WARNING("[TimeoutWorkflow] Interrupted");
    }

    void onFinalize() override {
        LOG_INFO("[TimeoutWorkflow] Completed");
    }

    Any onExecute() override {
        // 简单实现，实际执行由消息驱动
        return Any();
    }

private:
    int64_t timeoutMs_;
};

/**
 * @brief 带重试的工作流示例
 */
class RetryWorkflow : public BaseWorkflow {
public:
    RetryWorkflow(std::shared_ptr<IResourceManager> resourceManager,
                 std::shared_ptr<IRetryPolicy> retryPolicy)
        : BaseWorkflow("RetryWorkflow", resourceManager), retryPolicy_(retryPolicy),
          attemptCount_(0), shouldFail_(true) {}

    void setShouldFail(bool fail) { shouldFail_ = fail; }

protected:
    void onStart() override {
        LOG_INFO("[RetryWorkflow] Workflow started");
        attemptCount_ = 0;
        retryPolicy_->reset();
    }

    void onMessage(const IMessage& message) override {
        attemptCount_++;

        if (shouldFail_ && attemptCount_ < 3) {
            LOG_WARNING("[RetryWorkflow] Attempt " + std::to_string(attemptCount_) + " failed");

            if (retryPolicy_->shouldRetry(attemptCount_, "Simulated error", 0)) {
                LOG_INFO("[RetryWorkflow] Retrying...");
            } else {
                LOG_ERROR("[RetryWorkflow] Max retries reached");
                auto ctx = getContext();
                if (ctx) {
                    ctx->set("error", "Max retries exceeded");
                }
            }
        } else {
            LOG_INFO("[RetryWorkflow] Attempt " + std::to_string(attemptCount_) + " succeeded!");
            auto ctx = getContext();
            if (ctx) {
                ctx->set("success", "true");
            }
        }
    }

    void onInterrupt() override {
        LOG_WARNING("[RetryWorkflow] Interrupted");
    }

    void onFinalize() override {
        LOG_INFO("[RetryWorkflow] Completed after " + std::to_string(attemptCount_) + " attempts");
    }

    Any onExecute() override {
        // 简单实现，实际执行由消息驱动
        return Any();
    }

private:
    std::shared_ptr<IRetryPolicy> retryPolicy_;
    int attemptCount_;
    bool shouldFail_;
};

/**
 * @brief 带监控的工作流示例
 */
class MonitoredWorkflow : public BaseWorkflow {
public:
    MonitoredWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("MonitoredWorkflow", resourceManager),
          messageCount_(0) {}

    void setMessageCount(int count) { messageCount_ = count; }

protected:
    void onStart() override {
        LOG_INFO("[MonitoredWorkflow] Workflow started");
        processedCount_ = 0;
    }

    void onMessage(const IMessage& message) override {
        processedCount_++;
        LOG_INFO("[MonitoredWorkflow] Processing message " +
                 std::to_string(processedCount_) + "/" + std::to_string(messageCount_));

        // 模拟处理
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void onInterrupt() override {
        LOG_WARNING("[MonitoredWorkflow] Interrupted at message " +
                   std::to_string(processedCount_));
    }

    void onFinalize() override {
        LOG_INFO("[MonitoredWorkflow] Completed. Processed " +
                 std::to_string(processedCount_) + " messages");
    }

    Any onExecute() override {
        // 简单实现，实际执行由消息驱动
        return Any();
    }

private:
    int messageCount_;
    int processedCount_;
};

/**
 * @brief 主函数
 */
int main() {
    // 初始化系统
    auto& system = SystemFacade::getInstance();
    system.initialize("/tmp/workflow_advanced_resources");

    auto manager = system.getManager();
    auto resourceManager = system.getResourceManager();

    // 获取指标收集器
    auto& metricsCollector = MetricsCollector::getInstance();

    // ========== 测试1: 超时控制 ==========
    std::cout << "\n========== 测试1: 超时控制 ==========" << std::endl;

    // 创建超时工作流
    auto timeoutWorkflow = std::make_shared<TimeoutWorkflow>(resourceManager, 1000);

    // 配置超时处理器
    TimeoutConfig timeoutConfig;
    timeoutConfig.timeoutMs = 1000;
    timeoutConfig.policy = TimeoutPolicy::INTERRUPT;
    timeoutConfig.callback = [](const std::string& name, int64_t elapsed) {
        std::cout << "[Callback] Workflow " << name
                  << " timed out after " << elapsed << "ms" << std::endl;
    };
    timeoutConfig.autoRestart = false;
    timeoutConfig.maxRetries = 3;

    auto timeoutHandler = TimeoutHandlerFactory::createWithConfig(
        "TimeoutWorkflow",
        timeoutWorkflow,
        timeoutConfig
    );

    manager->registerWorkflow("TimeoutWorkflow", timeoutWorkflow);
    manager->startWorkflow("TimeoutWorkflow");

    // 发送消息模拟工作
    for (int i = 0; i < 5; ++i) {
        auto msg = SimpleMessage("test", "Message " + std::to_string(i));
        manager->handleMessage(msg);

        // 检查是否超时
        if (timeoutHandler->isTimedOut()) {
            std::cout << "Workflow timed out!" << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // ========== 测试2: 重试机制 ==========
    std::cout << "\n========== 测试2: 重试机制 ==========" << std::endl;

    // 创建重试策略（指数退避）
    auto retryPolicy = RetryPolicyFactory::createExponentialBackoff(
        3,           // 最大重试次数
        500,          // 初始延迟
        2.0,          // 退避乘数
        2000          // 最大延迟
    );

    auto retryWorkflow = std::make_shared<RetryWorkflow>(resourceManager, retryPolicy);
    manager->registerWorkflow("RetryWorkflow", retryWorkflow);
    manager->startWorkflow("RetryWorkflow");

    // 发送消息触发重试
    auto msg = SimpleMessage("test", "Trigger retry");
    manager->handleMessage(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // ========== 测试3: 指标收集 ==========
    std::cout << "\n========== 测试3: 指标收集 ==========" << std::endl;

    auto monitoredWorkflow = std::make_shared<MonitoredWorkflow>(resourceManager);
    monitoredWorkflow->setMessageCount(10);
    manager->registerWorkflow("MonitoredWorkflow", monitoredWorkflow);

    // 运行几次工作流收集指标
    for (int run = 1; run <= 3; ++run) {
        std::cout << "\n--- 运行 #" << run << " ---" << std::endl;

        manager->startWorkflow("MonitoredWorkflow");

        for (int i = 0; i < 5; ++i) {
            auto msg = SimpleMessage("test", "Message " + std::to_string(i));
            manager->handleMessage(msg);
        }

        manager->getCurrentWorkflow()->finish();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // ========== 显示指标报告 ==========
    std::cout << "\n========== 指标报告 ==========" << std::endl;

    // 生成摘要报告
    std::cout << "\n--- 摘要报告 ---" << std::endl;
    std::cout << metricsCollector.generateReport(MetricsReportType::SUMMARY);

    // 生成详细报告
    std::cout << "\n--- 详细报告 ---" << std::endl;
    std::cout << metricsCollector.generateReport(MetricsReportType::DETAILED);

    // 生成 JSON 报告
    std::cout << "\n--- JSON 报告 ---" << std::endl;
    std::cout << metricsCollector.generateReport(MetricsReportType::JSON);

    // 导出到文件
    std::cout << "\n--- 导出报告 ---" << std::endl;
    metricsCollector.exportMetrics("/tmp/workflow_metrics.json", MetricsReportType::JSON);
    metricsCollector.exportMetrics("/tmp/workflow_metrics.csv", MetricsReportType::CSV);
    std::cout << "Metrics exported to /tmp/workflow_metrics.json and .csv" << std::endl;

    // ========== 获取统计信息 ==========
    std::cout << "\n========== 统计信息 ==========" << std::endl;

    auto stats = metricsCollector.getStatistics("MonitoredWorkflow");
    std::cout << "MonitoredWorkflow Statistics:" << std::endl;
    std::cout << "  Total Executions: " << stats.totalExecutions << std::endl;
    std::cout << "  Success Rate: " << (stats.successRate * 100) << "%" << std::endl;
    std::cout << "  Average Duration: " << stats.averageDuration << "ms" << std::endl;
    std::cout << "  Min Duration: " << stats.minDuration << "ms" << std::endl;
    std::cout << "  Max Duration: " << stats.maxDuration << "ms" << std::endl;
    std::cout << "  Total Messages: " << stats.totalMessages << std::endl;

    // 清理
    std::cout << "\n========== 清理 ==========" << std::endl;
    manager->cleanupAllResources();
    system.shutdown();

    std::cout << "\n示例程序执行完成！" << std::endl;
    return 0;
}
