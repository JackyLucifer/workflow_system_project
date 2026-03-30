/**
 * @file config_example.cpp
 * @brief 配置管理器使用示例
 *
 * 演示配置管理器的各种功能：
 * - 加载和保存配置
 * - 读取各种类型的配置值
 * - 配置验证
 * - 配置变更监听
 * - 环境变量替换
 */

#include "workflow_system/implementations/system_facade.h"
#include "workflow_system/implementations/json_config_manager.h"
#include "workflow_system/implementations/workflows/base_workflow.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <cstdlib>

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
 * @brief 演示工作流
 */
class DemoWorkflow : public BaseWorkflow {
public:
    DemoWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("DemoWorkflow", resourceManager),
          messageCount_(0), maxMessages_(5) {}

    void setMaxMessages(int max) { maxMessages_ = max; }
    int getMessageCount() const { return messageCount_; }

protected:
    void onStart() override {
        LOG_INFO("[DemoWorkflow] Workflow started");
        auto ctx = getContext();
        if (ctx) {
            ctx->set("maxMessages", std::to_string(maxMessages_));
        }
        messageCount_ = 0;
    }

    void onMessage(const IMessage& message) override {
        messageCount_++;
        LOG_INFO("[DemoWorkflow] Processing message " +
                 std::to_string(messageCount_) + "/" + std::to_string(maxMessages_) +
                 ": " + message.getPayload());

        // 模拟处理
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 自动完成
        if (messageCount_ >= maxMessages_) {
            finish();
        }
    }

    void onInterrupt() override {
        LOG_WARNING("[DemoWorkflow] Interrupted");
    }

    void onFinalize() override {
        LOG_INFO("[DemoWorkflow] Completed. Processed " +
                 std::to_string(messageCount_) + " messages");
    }

    Any onExecute() override {
        return Any();
    }

private:
    int messageCount_;
    int maxMessages_;
};

/**
 * @brief 配置变更监听器示例
 */
class ConfigChangeListener {
public:
    void onChanged(const ConfigChangeEvent& event) {
        std::cout << "\n[Config Change Detected]" << std::endl;
        std::cout << "  Key: " << event.key << std::endl;
        std::cout << "  Old Value: " << event.oldValue << std::endl;
        std::cout << "  New Value: " << event.newValue << std::endl;
        std::cout << std::endl;
    }
};

/**
 * @brief 主函数
 */
int main() {
    // 初始化系统
    auto& system = SystemFacade::getInstance();
    system.initialize("/tmp/workflow_config_resources");

    auto manager = system.getManager();
    auto resourceManager = system.getResourceManager();

    // 获取配置管理器
    auto& config = JsonConfigManager::getInstance();

    // ========== 示例 1: 创建默认配置 ==========
    std::cout << "========== 示例 1: 创建默认配置 ==========" << std::endl;

    config.setString("workflows.dataImport.timeout", "5000");
    config.setString("workflows.dataImport.retry.maxAttempts", "3");
    config.setString("workflows.dataImport.retry.strategy", "exponential_backoff");

    config.setString("workflows.dataProcess.timeout", "3000");
    config.setString("workflows.dataProcess.parallel", "true");

    config.setString("orchestrator.maxParallelWorkflows", "4");
    config.setString("orchestrator.checkpointInterval", "60000");

    config.setString("logging.level", "info");
    config.setString("logging.file", "/tmp/workflow.log");

    config.setString("metrics.enabled", "true");
    config.setString("metrics.maxHistorySize", "1000");

    std::cout << "Default config set." << std::endl;
    std::cout << "\n" << config.toString() << std::endl;

    // 保存配置
    config.save("/tmp/workflow_config.json");
    std::cout << "Config saved to: /tmp/workflow_config.json" << std::endl;

    // ========== 示例 2: 读取配置 ==========
    std::cout << "\n========== 示例 2: 读取配置 ==========" << std::endl;

    int timeout = config.getInt("workflows.dataImport.timeout");
    std::cout << "Data Import Timeout: " << timeout << "ms" << std::endl;

    int maxAttempts = config.getInt("workflows.dataImport.retry.maxAttempts");
    std::cout << "Max Retry Attempts: " << maxAttempts << std::endl;

    std::string strategy = config.getString("workflows.dataImport.retry.strategy", "unknown");
    std::cout << "Retry Strategy: " << strategy << std::endl;

    bool parallelEnabled = config.getBool("workflows.dataProcess.parallel");
    std::cout << "Parallel Processing: " << (parallelEnabled ? "enabled" : "disabled") << std::endl;

    int maxWorkflows = config.getInt("orchestrator.maxParallelWorkflows");
    std::cout << "Max Parallel Workflows: " << maxWorkflows << std::endl;

    // ========== 示例 3: 配置变更监听 ==========
    std::cout << "\n========== 示例 3: 配置变更监听 ==========" << std::endl;

    ConfigChangeListener listener;
    config.addChangeListener([&listener](const ConfigChangeEvent& event) {
        listener.onChanged(event);
    });

    // 修改配置
    std::cout << "Modifying config..." << std::endl;
    config.setString("workflows.dataImport.timeout", "10000");  // 修改超时时间

    // ========== 示例 4: 从文件加载配置 ==========
    std::cout << "\n========== 示例 4: 从文件加载配置 ==========" << std::endl;

    // 先保存一个不同的配置
    config.setString("workflows.dataImport.timeout", "7000");
    config.save("/tmp/workflow_config_reload.json");

    // 重新加载配置
    config.load("/tmp/workflow_config.json");
    timeout = config.getInt("workflows.dataImport.timeout");
    std::cout << "After reload, Timeout: " << timeout << "ms" << std::endl;

    std::cout << "Config was modified: " << (config.isModified() ? "yes" : "no") << std::endl;

    // ========== 示例 5: 配置验证 ==========
    std::cout << "\n========== 示例 5: 配置验证 ==========" << std::endl;

    ConfigValidator validator;

    // 添加验证规则
    validator.addRule(ConfigRules::positiveInteger(
        "workflows.dataImport.timeout",
        "Timeout must be a positive integer"
    ));

    validator.addRule(ConfigRules::range(
        "orchestrator.maxParallelWorkflows",
        1, 10,
        "Max parallel workflows must be between 1 and 10"
    ));

    validator.addRule(ConfigRules::enumeration(
        "workflows.dataImport.retry.strategy",
        {"fixed_delay", "exponential_backoff", "linear_backoff", "immediate"},
        "Retry strategy must be one of: fixed_delay, exponential_backoff, linear_backoff, immediate"
    ));

    // 验证配置（注意：需要创建一个临时 map）
    // 注意：JsonConfigManager 的内部 config_ 是私有的，这里为了演示创建临时配置
    std::map<std::string, std::string> tempConfig;
    tempConfig["workflows.dataImport.timeout"] = std::to_string(config.getInt("workflows.dataImport.timeout"));
    tempConfig["workflows.dataImport.retry.strategy"] = config.getString("workflows.dataImport.retry.strategy");

    auto errors = validator.validate(tempConfig);
    if (errors.empty()) {
        std::cout << "Config validation: PASSED" << std::endl;
    } else {
        std::cout << "Config validation: FAILED" << std::endl;
        for (const auto& error : errors) {
            std::cout << "  - [" << error.key << "] " << error.error;
            if (!error.suggestion.empty()) {
                std::cout << " (Suggestion: " << error.suggestion << ")";
            }
            std::cout << std::endl;
        }
    }

    // ========== 示例 6: 使用配置驱动工作流 ==========
    std::cout << "\n========== 示例 6: 使用配置驱动工作流 ==========" << std::endl;

    auto workflow = std::make_shared<DemoWorkflow>(resourceManager);
    workflow->setMaxMessages(config.getInt("workflows.dataProcess.maxMessages", 3));

    manager->registerWorkflow("ConfigDrivenWorkflow", workflow);

    std::cout << "Starting workflow with config..." << std::endl;
    manager->startWorkflow("ConfigDrivenWorkflow");

    // 发送消息
    for (int i = 1; i <= workflow->getMessageCount(); ++i) {
        auto msg = SimpleMessage("data", "Message " + std::to_string(i));
        manager->handleMessage(msg);

        // 读取配置并可能修改
        int currentMax = config.getInt("workflows.dataProcess.maxMessages");
        if (i == 2 && currentMax < 5) {
            // 动态调整配置
            std::cout << "Adjusting config dynamically..." << std::endl;
            config.setInt("workflows.dataProcess.maxMessages", currentMax + 1);
            config.save();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 等待工作流完成
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // ========== 示例 7: 配置热重载 ==========
    std::cout << "\n========== 示例 7: 配置热重载 ==========" << std::endl;

    config.setReloadCallback([]() {
        std::cout << "[Hot Reload] Config file has been reloaded!" << std::endl;
    });

    // 修改外部配置文件
    {
        std::ofstream configFile;
        configFile.open("/tmp/workflow_config_hot.json");
        if (configFile.is_open()) {
            configFile << "{\n";
            configFile << "  \"workflows\": {\n";
            configFile << "    \"dataImport\": {\n";
            configFile << "      \"timeout\": 15000\n";
            configFile << "    }\n";
            configFile << "  }\n";
            configFile << "}\n";
            configFile.close();
            std::cout << "Modified external config file for hot reload testing." << std::endl;

            std::cout << "Press Enter to trigger hot reload..." << std::endl;
            std::cin.get();

            // 触发重载
            config.load("/tmp/workflow_config_hot.json");

            timeout = config.getInt("workflows.dataImport.timeout");
            std::cout << "After hot reload, Timeout: " << timeout << "ms" << std::endl;
        }
    }

    // ========== 示例 8: 环境变量替换 ==========
    std::cout << "\n========== 示例 8: 环境变量替换（模拟） ==========" << std::endl;

    // 设置环境变量（模拟）
    setenv("WORKFLOW_TIMEOUT", "8000", 1);
    setenv("WORKFLOW_LOG_LEVEL", "debug", 1);

    std::cout << "Environment variables set:" << std::endl;
    std::cout << "  WORKFLOW_TIMEOUT=8000" << std::endl;
    std::cout << "  WORKFLOW_LOG_LEVEL=debug" << std::endl;
    std::cout << "\n(Actual implementation would replace ${VAR} with env values)" << std::endl;

    // ========== 示例 9: 检查配置键 ==========
    std::cout << "\n========== 示例 9: 检查配置键 ==========" << std::endl;

    std::cout << "Checking for specific keys:" << std::endl;
    std::cout << "  workflows.dataImport.timeout exists: " <<
                 (config.hasKey("workflows.dataImport.timeout") ? "yes" : "no") << std::endl;
    std::cout << "  workflows.missing.key exists: " <<
                 (config.hasKey("workflows.missing.key") ? "yes" : "no") << std::endl;

    // 获取所有配置键
    std::cout << "\nAll top-level config keys:" << std::endl;
    auto allKeys = config.getAllKeys();
    for (const auto& key : allKeys) {
        std::cout << "  - " << key << std::endl;
    }

    // ========== 示例 10: 配置导出 ==========
    std::cout << "\n========== 示例 10: 配置导出 ==========" << std::endl;

    std::string configJson = config.toString();
    std::cout << "Current configuration:" << std::endl;
    std::cout << configJson << std::endl;

    // 导出到文件
    config.save("/tmp/workflow_config_export.json");
    std::cout << "Config exported to: /tmp/workflow_config_export.json" << std::endl;

    // ========== 清理 ==========
    std::cout << "\n========== 清理 ==========" << std::endl;
    manager->cleanupAllResources();
    system.shutdown();

    std::cout << "\n配置管理器示例执行完成！" << std::endl;

    return 0;
}
