#ifndef CALCULATOR_PLUGIN_HPP
#define CALCULATOR_PLUGIN_HPP

#include "workflow_system/plugin/core/IPlugin.hpp"
#include "workflow_system/plugin/core/IPluginContext.hpp"
#include "workflow_system/plugin/communication/Message.hpp"
#include "workflow_system/plugin/communication/Event.hpp"
#include <map>
#include <string>

using namespace WorkflowSystem::Plugin;

/**
 * @brief 计算器插件示例
 *
 * 演示如何实现一个简单的插件，提供基本的计算功能
 */
class CalculatorPlugin : public IPlugin {
public:
    CalculatorPlugin() = default;
    ~CalculatorPlugin() override = default;

    PluginSpec getSpec() const override {
        PluginSpec spec;
        spec.id = "com.example.calculator";
        spec.name = "Calculator Plugin";
        spec.description = "提供基本计算功能的插件";
        spec.version = Version(1, 0, 0);
        spec.author = "Example";
        spec.license = "MIT";
        spec.homepage = "https://example.com";
        return spec;
    }

    PluginInstanceInfo getInstanceInfo() const override {
        PluginInstanceInfo info;
        info.spec = getSpec();
        info.state = state_;
        return info;
    }

    bool onLoad(IPluginContext* context) override {
        context_ = context;
        context_->logInfo("计算器插件加载中...");

        // 订阅事件
        context_->subscribe("calculator.calculate", [this](const IEvent& event) {
            handleCalculateEvent(event);
        });

        return true;
    }

    void onUnload() override {
        if (context_) {
            context_->logInfo("计算器插件卸载中...");
        }
    }

    bool onInitialize() override {
        if (context_) {
            context_->logInfo("计算器插件初始化完成");
        }
        return true;
    }

    bool onStart() override {
        state_ = PluginState::RUNNING;
        if (context_) {
            context_->logInfo("计算器插件已启动");
            Event event("plugin.started", getSpec().id);
            context_->publishAsync(event);
        }
        return true;
    }

    void onStop() override {
        state_ = PluginState::STOPPED;
        if (context_) {
            context_->logInfo("计算器插件已停止");
        }
    }

    void onCleanup() override {
        context_ = nullptr;
    }

    bool loadConfig(const std::string& config) override {
        return true;
    }

    std::string getConfig() const override {
        return "{}";
    }

    void onConfigChanged(const std::string& key) override {
        if (context_) {
            context_->logInfo("配置变更: " + key);
        }
    }

    bool validateConfig(const std::string& config) const override {
        return true;
    }

    std::vector<Dependency> getDependencies() const override {
        return {};
    }

    bool onDependencyLoaded(const std::string& pluginId) override {
        return true;
    }

    void onDependencyUnloaded(const std::string& pluginId) override {
    }

    bool checkDependencies() const override {
        return true;
    }

    Response onMessage(const Message& message) override {
        const std::string& method = message.method();

        double a = message.payload().getDouble("a", 0.0);
        double b = message.payload().getDouble("b", 0.0);
        double result = 0.0;
        bool success = true;
        std::string errorMsg;

        if (method == "add") {
            result = a + b;
        } else if (method == "subtract") {
            result = a - b;
        } else if (method == "multiply") {
            result = a * b;
        } else if (method == "divide") {
            if (b == 0.0) {
                success = false;
                errorMsg = "除数不能为零";
            } else {
                result = a / b;
            }
        } else {
            return Response::error("未知方法: " + method);
        }

        operationCount_++;

        if (!success) {
            return Response::error(errorMsg);
        }

        Response response = Response::ok("计算成功");
        response.setData("result", std::to_string(result));
        return response;
    }

    void onEvent(const IEvent& event) override {
    }

    void onDataReceived(const std::vector<uint8_t>& data) override {
    }

    std::string callMethod(const std::string& method,
                           const std::string& params) override {
        return "{\"result\": 0}";
    }

    std::vector<std::string> getSupportedMethods() const override {
        return {"add", "subtract", "multiply", "divide"};
    }

    HealthStatus getHealthStatus() const override {
        HealthStatus status;
        status.status = HealthStatus::HEALTHY;
        status.message = "运行正常";
        return status;
    }

    PluginMetrics getMetrics() const override {
        PluginMetrics metrics;
        metrics.messagesProcessed = operationCount_;
        return metrics;
    }

    bool healthCheck() const override {
        return true;
    }

    std::string getDebugInfo() const override {
        return "CalculatorPlugin v1.0.0 - 已执行操作: " + std::to_string(operationCount_);
    }

    std::string getDiagnostics() const override {
        return "{\"status\": \"healthy\", \"operations\": " + std::to_string(operationCount_) + "}";
    }

    std::string getStatus() const override {
        return "Running";
    }

private:
    IPluginContext* context_ = nullptr;
    PluginState state_ = PluginState::LOADED;
    size_t operationCount_ = 0;

    void handleCalculateEvent(const IEvent& event) {
        const auto& data = event.getData();

        auto opIt = data.find("operation");
        if (opIt == data.end()) {
            return;
        }

        const std::string& operation = opIt->second;
        double a = std::stod(data.count("a") ? data.at("a") : "0");
        double b = std::stod(data.count("b") ? data.at("b") : "0");

        double result = 0.0;

        if (operation == "add") {
            result = a + b;
        } else if (operation == "subtract") {
            result = a - b;
        } else if (operation == "multiply") {
            result = a * b;
        } else if (operation == "divide") {
            if (b != 0.0) {
                result = a / b;
            }
        }

        operationCount_++;

        if (context_) {
            EventData resultData;
            resultData["result"] = std::to_string(result);
            Event resultEvent("calculator.result", getSpec().id, resultData);
            context_->publishAsync(resultEvent);
        }
    }
};

extern "C" {
    IPlugin* createPlugin() {
        return new CalculatorPlugin();
    }

    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}

#endif // CALCULATOR_PLUGIN_HPP
