/**
 * @file hello_plugin.cpp
 * @brief 示例插件 - Hello World
 */

#include "plugin_interface.h"
#include "service_registry.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <algorithm>

namespace WorkflowSystem {
namespace Plugin {

class HelloPlugin : public IPlugin {
public:
    HelloPlugin() 
        : state_(PluginState::UNLOADED)
        , context_(nullptr) {}
    
    // ========== 生命周期 ==========
    
    PluginResult initialize(IPluginContext* context) override {
        context_ = context;
        state_ = PluginState::INITIALIZED;
        
        if (context_) {
            context_->logInfo("HelloPlugin 初始化完成");
        }
        
        return PluginResult::ok("初始化成功");
    }
    
    PluginResult start() override {
        if (state_ != PluginState::INITIALIZED) {
            return PluginResult::error(-1, "插件未初始化");
        }
        
        state_ = PluginState::RUNNING;
        
        if (context_) {
            context_->logInfo("HelloPlugin 启动成功");
        }
        
        return PluginResult::ok("启动成功");
    }
    
    PluginResult stop() override {
        if (state_ != PluginState::RUNNING) {
            return PluginResult::ok("插件未运行");
        }
        
        state_ = PluginState::STOPPED;
        
        if (context_) {
            context_->logInfo("HelloPlugin 已停止");
        }
        
        return PluginResult::ok("停止成功");
    }
    
    PluginResult unload() override {
        state_ = PluginState::UNLOADED;
        context_ = nullptr;
        return PluginResult::ok("卸载成功");
    }
    
    // ========== 信息获取 ==========
    
    PluginMetadata getMetadata() const override {
        PluginMetadata meta;
        meta.name = "hello_plugin";
        meta.displayName = "Hello World Plugin";
        meta.description = "一个简单的示例插件";
        meta.author = "WorkflowSystem";
        meta.license = "MIT";
        meta.version = PluginVersion(1, 0, 0);
        meta.tags = {"demo", "hello"};
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
        
        if (action == "greet") {
            return handleGreet(params);
        } else if (action == "echo") {
            return handleEcho(params);
        } else if (action == "use_service") {
            return handleUseService(params);
        }
        
        return PluginResult::error(-2, "不支持的动作: " + action);
    }
    
    std::vector<std::string> getSupportedActions() const override {
        return {"greet", "echo", "use_service"};
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
    
    PluginResult handleGreet(const PluginParams& params) {
        std::string name = params.getString("name", "World");
        std::string greeting = "Hello, " + name + "!";
        
        if (context_) {
            context_->logInfo(greeting);
        }
        
        return PluginResult::ok("问候成功", greeting);
    }
    
    PluginResult handleEcho(const PluginParams& params) {
        std::string message = params.getString("message", "");
        
        std::ostringstream oss;
        oss << "Echo: " << message;
        oss << " (int=" << params.getInt("count", 0);
        oss << ", bool=" << (params.getBool("flag", false) ? "true" : "false") << ")";
        
        return PluginResult::ok("回显成功", oss.str());
    }
    
    PluginResult handleUseService(const PluginParams& params) {
        std::string serviceName = params.getString("service", "");
        
        if (serviceName.empty()) {
            return PluginResult::error(-3, "未指定服务名称");
        }
        
        // 从服务注册中心获取服务
        void* service = ServiceRegistry::getInstance().getRaw(serviceName);
        
        if (service) {
            std::ostringstream oss;
            oss << "成功获取服务: " << serviceName;
            oss << " (地址: " << service << ")";
            
            if (context_) {
                context_->logInfo(oss.str());
            }
            
            return PluginResult::ok("服务获取成功", oss.str());
        }
        
        return PluginResult::error(-4, "服务不存在: " + serviceName);
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

// 导出插件
DEFINE_PLUGIN(WorkflowSystem::Plugin::HelloPlugin)
