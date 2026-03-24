/**
 * @file advanced_plugin.cpp
 * @brief 高级示例插件 - 展示依赖管理、消息通信、配置系统
 */

#include "plugin_interface.h"
#include "service_registry.h"
#include "message_bus.h"
#include "plugin_config.h"
#include "resource_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace WorkflowSystem {
namespace Plugin {

class AdvancedPlugin : public IPlugin {
public:
    AdvancedPlugin() 
        : state_(PluginState::UNLOADED)
        , context_(nullptr)
        , subscriptionId_(0)
        , buffer_(nullptr)
        , resourceId_(0) {}
    
    // ========== 生命周期 ==========
    
    PluginResult initialize(IPluginContext* context) override {
        context_ = context;
        state_ = PluginState::INITIALIZED;
        
        // 订阅消息
        subscriptionId_ = MessageBus::getInstance().subscribe(
            MessageTopics::PLUGIN_LOADED,
            [this](const Message& msg) {
                if (context_) {
                    std::string info = "收到消息: " + msg.topic + " from " + msg.source;
                    context_->logInfo(info);
                }
            },
            "advanced_plugin"
        );
        
        // 注册资源
        buffer_ = new char[1024];
        resourceId_ = ResourceManager::getInstance().registerMemory(
            buffer_, 1024, "advanced_plugin", "示例缓冲区"
        );
        
        if (context_) {
            context_->logInfo("AdvancedPlugin 初始化完成");
        }
        
        return PluginResult::ok("初始化成功");
    }
    
    PluginResult start() override {
        if (state_ != PluginState::INITIALIZED) {
            return PluginResult::error(-1, "插件未初始化");
        }
        
        state_ = PluginState::RUNNING;
        
        if (context_) {
            context_->logInfo("AdvancedPlugin 启动成功");
        }
        
        // 发布启动消息
        Message msg = Message::create(MessageTopics::PLUGIN_STARTED, "started");
        msg.source = "advanced_plugin";
        MessageBus::getInstance().publish(msg);
        
        return PluginResult::ok("启动成功");
    }
    
    PluginResult stop() override {
        if (state_ != PluginState::RUNNING) {
            return PluginResult::ok("插件未运行");
        }
        
        state_ = PluginState::STOPPED;
        
        if (context_) {
            context_->logInfo("AdvancedPlugin 已停止");
        }
        
        // 发布停止消息
        Message msg = Message::create(MessageTopics::PLUGIN_STOPPED, "stopped");
        msg.source = "advanced_plugin";
        MessageBus::getInstance().publish(msg);
        
        return PluginResult::ok("停止成功");
    }
    
    PluginResult unload() override {
        // 取消订阅
        if (subscriptionId_ > 0) {
            MessageBus::getInstance().unsubscribe(subscriptionId_);
            subscriptionId_ = 0;
        }
        
        // 释放资源
        if (resourceId_ > 0) {
            ResourceManager::getInstance().releaseResource(resourceId_);
            resourceId_ = 0;
        }
        
        if (buffer_) {
            delete[] buffer_;
            buffer_ = nullptr;
        }
        
        state_ = PluginState::UNLOADED;
        context_ = nullptr;
        
        return PluginResult::ok("卸载成功");
    }
    
    // ========== 信息获取 ==========
    
    PluginMetadata getMetadata() const override {
        PluginMetadata meta;
        meta.name = "advanced_plugin";
        meta.displayName = "Advanced Plugin";
        meta.description = "展示高级功能的示例插件";
        meta.author = "WorkflowSystem";
        meta.license = "MIT";
        meta.version = PluginVersion(1, 0, 0);
        meta.dependencies = {"hello_plugin"};  // 依赖 hello_plugin
        meta.tags = {"demo", "advanced", "messaging", "config"};
        return meta;
    }
    
    PluginState getState() const override {
        return state_;
    }
    
    PluginConfig getConfig() const override {
        return config_;
    }
    
    void setConfig(const PluginConfig& config) override {
        config_ = config;
    }
    
    // ========== 功能执行 ==========
    
    PluginResult execute(const PluginParams& params) override {
        if (state_ != PluginState::RUNNING) {
            return PluginResult::error(-1, "插件未运行");
        }
        
        std::string action = params.action;
        
        if (action == "info") {
            return handleInfo(params);
        } else if (action == "publish") {
            return handlePublish(params);
        } else if (action == "config") {
            return handleConfig(params);
        } else if (action == "stats") {
            return handleStats(params);
        } else if (action == "resources") {
            return handleResources(params);
        }
        
        return PluginResult::error(-2, "不支持的动作: " + action);
    }
    
    std::vector<std::string> getSupportedActions() const override {
        return {"info", "publish", "config", "stats", "resources"};
    }
    
    bool supportsAction(const std::string& action) const override {
        auto actions = getSupportedActions();
        return std::find(actions.begin(), actions.end(), action) != actions.end();
    }
    
    // ========== 健康检查 ==========
    
    PluginResult healthCheck() override {
        if (state_ == PluginState::RUNNING) {
            return PluginResult::ok("健康");
        }
        return PluginResult::error(-1, "插件未运行");
    }

private:
    PluginState state_;
    IPluginContext* context_;
    PluginConfig config_;
    uint64_t subscriptionId_;
    char* buffer_;
    uint64_t resourceId_;
    
    PluginResult handleInfo(const PluginParams& params) {
        std::ostringstream oss;
        oss << "AdvancedPlugin v1.0.0\n";
        oss << "状态: " << static_cast<int>(state_) << "\n";
        oss << "订阅ID: " << subscriptionId_ << "\n";
        oss << "资源ID: " << resourceId_ << "\n";
        oss << "缓冲区: " << (buffer_ ? "已分配" : "未分配");
        
        return PluginResult::ok("信息", oss.str());
    }
    
    PluginResult handlePublish(const PluginParams& params) {
        std::string topic = params.getString("topic", "custom.event");
        std::string data = params.getString("data", "");
        
        Message msg = Message::create(topic, data);
        msg.source = "advanced_plugin";
        msg.priority = MessagePriority::NORMAL;
        
        MessageBus::getInstance().publish(msg);
        
        return PluginResult::ok("消息已发布", "topic=" + topic + ", data=" + data);
    }
    
    PluginResult handleConfig(const PluginParams& params) {
        std::string op = params.getString("op", "get");
        
        if (op == "set") {
            std::string key = params.getString("key", "");
            std::string value = params.getString("value", "");
            
            if (key.empty()) {
                return PluginResult::error(-2, "缺少 key 参数");
            }
            
            PluginConfigManager::getInstance()
                .setPluginConfig("advanced_plugin", key, ConfigValue::fromString(value));
            return PluginResult::ok("配置已设置", key + " = " + value);
        } else if (op == "get") {
            std::string key = params.getString("key", "");
            
            if (key.empty()) {
                return PluginResult::error(-2, "缺少 key 参数");
            }
            
            ConfigValue value = PluginConfigManager::getInstance()
                .getPluginConfig("advanced_plugin", key);
            
            return PluginResult::ok("配置值", key + " = " + value.asString());
        } else if (op == "list") {
            auto allConfig = PluginConfigManager::getInstance()
                .getAllPluginConfig("advanced_plugin");
            
            std::ostringstream oss;
            for (const auto& pair : allConfig) {
                oss << pair.first << " = " << pair.second.asString() << "\n";
            }
            
            return PluginResult::ok("所有配置", oss.str());
        }
        
        return PluginResult::error(-3, "未知的配置操作: " + op);
    }
    
    PluginResult handleStats(const PluginParams& params) {
        auto msgStats = MessageBus::getInstance().getStats();
        auto resStats = ResourceManager::getInstance().getActiveResources();
        
        std::ostringstream oss;
        oss << "=== 消息总线统计 ===\n";
        oss << "发送: " << msgStats.totalSent << "\n";
        oss << "投递: " << msgStats.totalDelivered << "\n";
        oss << "丢弃: " << msgStats.totalDropped << "\n";
        oss << "订阅者: " << msgStats.totalSubscribers << "\n";
        
        oss << "\n=== 资源管理统计 ===\n";
        oss << "活跃资源: " << resStats.size() << "\n";
        oss << "总内存: " << ResourceManager::getInstance().getTotalMemoryUsage() << " bytes\n";
        
        return PluginResult::ok("统计信息", oss.str());
    }
    
    PluginResult handleResources(const PluginParams& params) {
        auto resources = ResourceManager::getInstance()
            .getOwnerResources("advanced_plugin");
        
        std::ostringstream oss;
        oss << "=== AdvancedPlugin 资源 ===\n";
        for (const auto& res : resources) {
            oss << "ID: " << res.id << "\n";
            oss << "名称: " << res.name << "\n";
            oss << "类型: " << static_cast<int>(res.type) << "\n";
            oss << "大小: " << res.size << " bytes\n\n";
        }
        
        return PluginResult::ok("资源列表", oss.str());
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

// 导出插件
DEFINE_PLUGIN(WorkflowSystem::Plugin::AdvancedPlugin)
