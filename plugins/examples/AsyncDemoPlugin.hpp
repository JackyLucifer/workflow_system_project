#ifndef ASYNC_DEMO_PLUGIN_HPP
#define ASYNC_DEMO_PLUGIN_HPP

#include "workflow_system/plugin/core/IPlugin.hpp"
#include "workflow_system/plugin/core/IPluginContext.hpp"
#include "workflow_system/plugin/communication/Channel.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace WorkflowSystem::Plugin;

/**
 * @brief 异步通讯演示插件
 * 
 * 展示如何使用 Channel 进行插件间的异步通讯
 */
class AsyncDemoPlugin : public IPlugin {
public:
    AsyncDemoPlugin() = default;
    ~AsyncDemoPlugin() override = default;

    PluginSpec getSpec() const override {
        PluginSpec spec;
        spec.id = "com.example.async_demo";
        spec.name = "Async Demo Plugin";
        spec.description = "演示异步通讯的插件";
        spec.version = Version(1, 0, 0);
        spec.author = "Example";
        spec.license = "MIT";
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
        context_->logInfo("异步演示插件加载中...");

        // 获取默认通道
        auto channel = context_->getDefaultChannel();
        if (!channel) {
            context_->logError("无法获取默认通道");
            return false;
        }

        // 启动通道
        channel->start();

        // 订阅 "data.request" 主题
        subId_ = channel->subscribe("data.request", 
            [this](const DataPacket& packet) {
                handleDataRequest(packet);
            });

        // 也订阅 "data.broadcast" 主题
        channel->subscribe("data.broadcast",
            [this](const DataPacket& packet) {
                handleBroadcast(packet);
            });

        context_->logInfo("已订阅 data.request 和 data.broadcast 主题");
        return true;
    }

    void onUnload() override {
        if (context_) {
            context_->logInfo("异步演示插件卸载中...");
            auto channel = context_->getDefaultChannel();
            if (channel) {
                channel->unsubscribe(subId_);
            }
        }
    }

    bool onInitialize() override {
        if (context_) {
            context_->logInfo("异步演示插件初始化完成");
        }
        return true;
    }

    bool onStart() override {
        state_ = PluginState::RUNNING;
        if (context_) {
            context_->logInfo("异步演示插件已启动");

            // 演示：发送一个广播消息
            std::map<std::string, std::string> data;
            data["message"] = "插件已启动";
            data["plugin"] = getSpec().id;
            context_->publishData("data.broadcast", data);
        }
        return true;
    }

    void onStop() override {
        state_ = PluginState::STOPPED;
        if (context_) {
            context_->logInfo("异步演示插件已停止");
        }
    }

    void onCleanup() override {
        context_ = nullptr;
    }

    bool loadConfig(const std::string& config) override { return true; }
    std::string getConfig() const override { return "{}"; }
    void onConfigChanged(const std::string& key) override {}
    bool validateConfig(const std::string& config) const override { return true; }

    std::vector<Dependency> getDependencies() const override { return {}; }
    bool onDependencyLoaded(const std::string& pluginId) override { return true; }
    void onDependencyUnloaded(const std::string& pluginId) override {}
    bool checkDependencies() const override { return true; }

    Response onMessage(const Message& message) override {
        const std::string& method = message.method();

        if (method == "ping") {
            // 使用 Channel 发布一个事件
            std::map<std::string, std::string> data;
            data["source"] = getSpec().id;
            data["timestamp"] = std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );
            context_->publishData("ping.received", data);

            Response response = Response::ok("pong");
            response.setData("time", data["timestamp"]);
            return response;
        }

        if (method == "send_async") {
            // 异步发送数据到指定主题
            std::string topic = message.payload().get("topic", "default");
            std::string content = message.payload().get("content", "");

            std::map<std::string, std::string> data;
            data["content"] = content;
            data["from"] = getSpec().id;
            context_->publishData(topic, data);

            return Response::ok("异步消息已发送到: " + topic);
        }

        if (method == "send_sync") {
            // 同步发送（直接触发回调）
            std::string topic = message.payload().get("topic", "default");
            std::string content = message.payload().get("content", "");

            std::map<std::string, std::string> data;
            data["content"] = content;
            data["from"] = getSpec().id;
            context_->publishDataSync(topic, data);

            return Response::ok("同步消息已发送到: " + topic);
        }

        if (method == "get_channel_info") {
            auto channel = context_->getDefaultChannel();
            if (!channel) {
                return Response::error("通道不可用");
            }

            Response response = Response::ok("通道信息");
            response.setData("name", channel->name());
            response.setData("subscribers", std::to_string(channel->subscriberCount()));
            response.setData("queue_size", std::to_string(channel->queueSize()));
            return response;
        }

        return Response::error("未知方法: " + method);
    }

    void onEvent(const IEvent& event) override {}
    void onDataReceived(const std::vector<uint8_t>& data) override {}

    std::string callMethod(const std::string& method, const std::string& params) override {
        return "{}";
    }

    std::vector<std::string> getSupportedMethods() const override {
        return {"ping", "send_async", "send_sync", "get_channel_info"};
    }

    HealthStatus getHealthStatus() const override {
        HealthStatus status;
        status.status = HealthStatus::HEALTHY;
        status.message = "运行正常";
        return status;
    }

    PluginMetrics getMetrics() const override {
        PluginMetrics metrics;
        metrics.messagesProcessed = messageCount_;
        return metrics;
    }

    bool healthCheck() const override { return true; }

    std::string getDebugInfo() const override {
        return "AsyncDemoPlugin - 已处理消息: " + std::to_string(messageCount_);
    }

    std::string getDiagnostics() const override {
        return "{\"status\": \"healthy\", \"messages\": " + std::to_string(messageCount_) + "}";
    }

    std::string getStatus() const override {
        return state_ == PluginState::RUNNING ? "Running" : "Stopped";
    }

private:
    IPluginContext* context_ = nullptr;
    PluginState state_ = PluginState::LOADED;
    SubscriptionId subId_ = 0;
    size_t messageCount_ = 0;

    void handleDataRequest(const DataPacket& packet) {
        messageCount_++;
        
        if (context_) {
            context_->logInfo("收到数据请求: topic=" + packet.topic + 
                            ", from=" + packet.source);

            // 处理请求并发送响应
            std::string operation = packet.get("operation", "echo");
            std::string value = packet.get("value", "");

            std::map<std::string, std::string> responseData;
            responseData["original"] = value;
            responseData["operation"] = operation;
            responseData["processed_by"] = getSpec().id;

            if (operation == "echo") {
                responseData["result"] = value;
            } else if (operation == "upper") {
                std::string upper = value;
                for (auto& c : upper) c = toupper(c);
                responseData["result"] = upper;
            } else if (operation == "lower") {
                std::string lower = value;
                for (auto& c : lower) c = tolower(c);
                responseData["result"] = lower;
            } else if (operation == "reverse") {
                std::string reversed = value;
                std::reverse(reversed.begin(), reversed.end());
                responseData["result"] = reversed;
            } else {
                responseData["result"] = "unknown operation";
            }

            // 发送响应到 "data.response" 主题
            context_->publishData("data.response", responseData);
        }
    }

    void handleBroadcast(const DataPacket& packet) {
        messageCount_++;
        
        if (context_) {
            context_->logInfo("收到广播消息: topic=" + packet.topic +
                            ", from=" + packet.source +
                            ", message=" + packet.get("message", ""));
        }
    }
};

extern "C" {
    IPlugin* createPlugin() {
        return new AsyncDemoPlugin();
    }

    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}

#endif // ASYNC_DEMO_PLUGIN_HPP
