/**
 * @file DemoPlugin.hpp
 * @brief 演示服务注入和异步通讯的示例插件
 */

#ifndef DEMO_PLUGIN_HPP
#define DEMO_PLUGIN_HPP

#include "workflow_system/plugin/core/IPlugin.hpp"
#include "workflow_system/plugin/core/IPluginContext.hpp"
#include "workflow_system/plugin/core/ServiceLocator.hpp"
#include "workflow_system/plugin/communication/Channel.hpp"
#include <iostream>
#include <ctime>

namespace WorkflowSystem { namespace Plugin {

// 定义一个日志服务接口（由框架/宿主程序实现并注入）
class ILogService {
public:
    virtual ~ILogService() {}
    virtual void info(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
};

// 定义一个配置服务接口
class IConfigService {
public:
    virtual ~IConfigService() {}
    virtual std::string get(const std::string& key, const std::string& def = "") = 0;
    virtual void set(const std::string& key, const std::string& value) = 0;
};

/**
 * @brief 演示插件 - 展示服务注入和异步通讯
 */
class DemoPlugin : public IPlugin {
public:
    DemoPlugin() : context_(nullptr), logService_(nullptr), configService_(nullptr) {}
    
    ~DemoPlugin() override {}
    
    // ==================== 基本信息 ====================
    
    PluginSpec getSpec() const override {
        PluginSpec spec;
        spec.name = "DemoPlugin";
        spec.version = Version(1, 0, 0);
        spec.description = "演示服务注入和异步通讯的插件";
        spec.author = "PluginFramework";
        return spec;
    }
    
    // ==================== 生命周期 ====================
    
    bool onLoad(IPluginContext* context) override {
        context_ = context;
        
        // 获取注入的日志服务
        logService_ = ServiceLocator::instance().getTyped<ILogService>("log_service");
        if (logService_) {
            logService_->info("DemoPlugin: 成功获取日志服务");
        } else {
            std::cout << "[DemoPlugin] 警告: 未找到日志服务" << std::endl;
        }
        
        // 获取注入的配置服务
        configService_ = ServiceLocator::instance().getTyped<IConfigService>("config_service");
        if (configService_) {
            std::string mode = configService_->get("mode", "default");
            if (logService_) {
                logService_->info("DemoPlugin: 运行模式 = " + mode);
            }
        }
        
        return true;
    }
    
    void onUnload() override {
        logService_ = nullptr;
        configService_ = nullptr;
        context_ = nullptr;
    }
    
    bool onInitialize() override {
        setupChannelSubscriptions();
        return true;
    }
    
    bool onStart() override {
        if (logService_) {
            logService_->info("DemoPlugin: 启动中...");
        }
        
        // 发布启动事件
        if (context_) {
            auto channel = context_->getDefaultChannel();
            if (channel) {
                DataPacket packet;
                packet.topic = "plugin.started";
                packet.source = getSpec().name;
                packet.set("plugin_name", getSpec().name);
                packet.set("timestamp", std::to_string(std::time(nullptr)));
                channel->publish(packet);
            }
        }
        
        return true;
    }
    
    void onStop() override {
        if (logService_) {
            logService_->info("DemoPlugin: 停止中...");
        }
        
        // 取消订阅
        for (auto id : subscriptionIds_) {
            if (context_) {
                context_->unsubscribeData(id);
            }
        }
        subscriptionIds_.clear();
    }
    
    // ==================== 通信接口 ====================
    
    Response onMessage(const Message& message) override {
        std::string method = message.method();
        if (method == "ping") {
            return Response::ok("pong");
        } else if (method == "status") {
            Response resp;
            resp.setStatus(Response::SUCCESS);
            resp.setData("status", "running");
            return resp;
        }
        return Response::error("未知消息类型: " + method);
    }
    
    void onEvent(const IEvent& event) override {
        if (logService_) {
            logService_->info("DemoPlugin: 收到事件 " + event.getName());
        }
    }

private:
    IPluginContext* context_;
    ILogService* logService_;
    IConfigService* configService_;
    std::vector<SubscriptionId> subscriptionIds_;
    
    void setupChannelSubscriptions() {
        if (!context_) return;
        
        // 订阅所有用户相关事件
        SubscriptionId id1 = context_->subscribeData("user.*", 
            [this](const DataPacket& packet) {
                handleUserEvent(packet);
            });
        if (id1 > 0) subscriptionIds_.push_back(id1);
        
        // 订阅系统命令
        SubscriptionId id2 = context_->subscribeData("system.command",
            [this](const DataPacket& packet) {
                handleSystemCommand(packet);
            });
        if (id2 > 0) subscriptionIds_.push_back(id2);
    }
    
    void handleUserEvent(const DataPacket& packet) {
        if (logService_) {
            std::string action = packet.get("action", "unknown");
            std::string userId = packet.get("user_id", "unknown");
            logService_->info("DemoPlugin: 收到用户事件 [" + packet.topic + "] action=" + action + ", user=" + userId);
        }
        
        // 触发其他事件
        if (context_ && packet.topic == "user.created") {
            auto channel = context_->getDefaultChannel();
            if (channel) {
                DataPacket response;
                response.topic = "notification.send";
                response.source = getSpec().name;
                response.set("type", "welcome");
                response.set("user_id", packet.get("user_id"));
                channel->publish(response);
            }
        }
    }
    
    void handleSystemCommand(const DataPacket& packet) {
        std::string cmd = packet.get("command", "");
        
        if (logService_) {
            logService_->info("DemoPlugin: 收到系统命令: " + cmd);
        }
        
        if (cmd == "status" && context_) {
            auto channel = context_->getDefaultChannel();
            if (channel) {
                DataPacket response;
                response.topic = "system.response";
                response.source = getSpec().name;
                response.set("status", "running");
                response.set("uptime", "ok");
                channel->publish(response);
            }
        }
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

extern "C" {
    WorkflowSystem::Plugin::IPlugin* createPlugin() {
        return new WorkflowSystem::Plugin::DemoPlugin();
    }

    void destroyPlugin(WorkflowSystem::Plugin::IPlugin* plugin) {
        delete plugin;
    }
}

#endif // DEMO_PLUGIN_HPP
