/**
 * @file advanced_plugin_loader.h
 * @brief 高级插件加载器 - 集成依赖解析、消息总线、配置系统
 */

#ifndef WORKFLOW_SYSTEM_ADVANCED_PLUGIN_LOADER_H
#define WORKFLOW_SYSTEM_ADVANCED_PLUGIN_LOADER_H

#include "plugin_interface.h"
#include "plugin_loader.h"
#include "dependency_resolver.h"
#include "message_bus.h"
#include "plugin_config.h"
#include "resource_manager.h"
#include "service_registry.h"
#include <memory>
#include <set>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 高级插件加载器配置
 */
struct AdvancedLoaderConfig {
    bool enableDependencyResolution;
    bool enableMessageBus;
    bool enableConfigSystem;
    bool enableResourceTracking;
    bool enableHotReload;
    int64_t hotReloadIntervalMs;
    bool enableVersionCheck;
    std::string configFilePath;
    
    AdvancedLoaderConfig()
        : enableDependencyResolution(true)
        , enableMessageBus(true)
        , enableConfigSystem(true)
        , enableResourceTracking(true)
        , enableHotReload(false)
        , hotReloadIntervalMs(5000)
        , enableVersionCheck(true) {}
};

/**
 * @brief 插件扩展信息
 */
struct ExtendedPluginInfo {
    std::string name;
    std::string path;
    PluginMetadata metadata;
    PluginState state;
    std::vector<std::string> dependencies;
    std::set<std::string> providedServices;
    std::set<std::string> usedServices;
    std::vector<uint64_t> subscriptions;
    std::vector<uint64_t> resources;
    int64_t loadTime;
    int loadOrder;
    bool isDependency;
    
    ExtendedPluginInfo()
        : state(PluginState::UNLOADED)
        , loadTime(0)
        , loadOrder(0)
        , isDependency(false) {}
};

/**
 * @brief 高级插件加载器
 */
class AdvancedPluginLoader {
public:
    using PluginEventCallback = std::function<void(const std::string& pluginName, const std::string& event, const std::string& data)>;
    
    AdvancedPluginLoader() 
        : basicLoader_()
        , config_() {}
    
    explicit AdvancedPluginLoader(const AdvancedLoaderConfig& config)
        : basicLoader_()
        , config_(config) {}
    
    ~AdvancedPluginLoader() {
        if (config_.enableResourceTracking) {
            cleanupAllResources();
        }
    }
    
    // ========== 配置 ==========
    
    void setConfig(const AdvancedLoaderConfig& config) {
        config_ = config;
    }
    
    AdvancedLoaderConfig getConfig() const {
        return config_;
    }
    
    void addSearchPath(const std::string& path) {
        basicLoader_.addSearchPath(path);
    }
    
    // ========== 依赖管理 ==========
    
    /**
     * @brief 注册插件依赖
     */
    void registerDependency(const std::string& pluginName, 
                           const std::vector<std::string>& dependencies,
                           int priority = 0) {
        resolver_.registerNode(pluginName, dependencies, priority);
        extendedInfo_[pluginName].dependencies = dependencies;
    }
    
    /**
     * @brief 解析依赖顺序
     */
    DependencyResult resolveDependencies() {
        return resolver_.resolve();
    }
    
    /**
     * @brief 获取加载顺序
     */
    std::vector<std::string> getLoadOrder() {
        auto result = resolver_.resolve();
        return result.success ? result.loadOrder : std::vector<std::string>();
    }
    
    // ========== 加载/卸载 ==========
    
    /**
     * @brief 加载插件（自动解析依赖）
     */
    bool load(const std::string& path) {
        if (!basicLoader_.load(path)) {
            return false;
        }
        
        auto loaded = basicLoader_.getLoadedPlugins();
        for (const auto& name : loaded) {
            if (extendedInfo_.find(name) == extendedInfo_.end()) {
                auto metadata = basicLoader_.getMetadata(name);
                extendedInfo_[name].name = name;
                extendedInfo_[name].metadata = metadata;
                extendedInfo_[name].state = PluginState::LOADED;
                extendedInfo_[name].loadTime = getCurrentTime();
                
                for (const auto& dep : metadata.dependencies) {
                    resolver_.registerNode(name, metadata.dependencies, 0);
                    extendedInfo_[name].dependencies = metadata.dependencies;
                }
                
                fireEvent(name, "loaded", path);
            }
        }
        
        return true;
    }
    
    /**
     * @brief 按依赖顺序加载所有插件
     */
    int loadAll() {
        auto discovered = basicLoader_.scanPlugins();
        
        for (const auto& path : discovered) {
            basicLoader_.load(path);
        }
        
        auto loaded = basicLoader_.getLoadedPlugins();
        for (const auto& name : loaded) {
            if (extendedInfo_.find(name) == extendedInfo_.end()) {
                auto metadata = basicLoader_.getMetadata(name);
                extendedInfo_[name].name = name;
                extendedInfo_[name].metadata = metadata;
                extendedInfo_[name].state = PluginState::LOADED;
                extendedInfo_[name].loadTime = getCurrentTime();
                
                if (!metadata.dependencies.empty()) {
                    resolver_.registerNode(name, metadata.dependencies, 0);
                    extendedInfo_[name].dependencies = metadata.dependencies;
                }
            }
        }
        
        return loaded.size();
    }
    
    /**
     * @brief 按依赖顺序初始化所有插件
     */
    int initializeAll(IPluginContext* context = nullptr) {
        auto result = resolver_.resolve();
        if (!result.success) {
            fireEvent("", "dependency_error", result.errorMessage);
            return 0;
        }
        
        int count = 0;
        int order = 0;
        for (const auto& name : result.loadOrder) {
            if (basicLoader_.initialize(name, context)) {
                extendedInfo_[name].state = PluginState::INITIALIZED;
                extendedInfo_[name].loadOrder = order++;
                
                if (config_.enableConfigSystem) {
                    loadPluginConfig(name);
                }
                
                fireEvent(name, "initialized", "");
                count++;
            }
        }
        
        return count;
    }
    
    /**
     * @brief 按依赖逆序停止所有插件
     */
    int stopAll() {
        auto result = resolver_.resolve();
        if (!result.success) {
            return basicLoader_.stopAll();
        }
        
        int count = 0;
        auto order = result.loadOrder;
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            if (basicLoader_.stop(*it)) {
                extendedInfo_[*it].state = PluginState::STOPPED;
                fireEvent(*it, "stopped", "");
                count++;
            }
        }
        
        return count;
    }
    
    /**
     * @brief 卸载插件
     */
    bool unload(const std::string& name) {
        if (config_.enableResourceTracking) {
            cleanupPluginResources(name);
        }
        
        if (config_.enableMessageBus) {
            unsubscribePlugin(name);
        }
        
        if (basicLoader_.unload(name)) {
            extendedInfo_.erase(name);
            resolver_.removeNode(name);
            fireEvent(name, "unloaded", "");
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief 卸载所有插件
     */
    void unloadAll() {
        auto result = resolver_.resolve();
        if (result.success) {
            auto order = result.loadOrder;
            for (auto it = order.rbegin(); it != order.rend(); ++it) {
                unload(*it);
            }
        } else {
            basicLoader_.unloadAll();
            extendedInfo_.clear();
            resolver_.clear();
        }
    }
    
    // ========== 消息总线集成 ==========
    
    /**
     * @brief 订阅消息
     */
    uint64_t subscribe(const std::string& pluginName,
                      const std::string& topic,
                      MessageHandler handler) {
        uint64_t subId = MessageBus::getInstance().subscribe(topic, handler, pluginName);
        extendedInfo_[pluginName].subscriptions.push_back(subId);
        return subId;
    }
    
    /**
     * @brief 发布消息
     */
    void publish(const std::string& pluginName, const Message& message) {
        Message msg = message;
        msg.source = pluginName;
        MessageBus::getInstance().publish(msg);
    }
    
    /**
     * @brief 发布消息（简化版）
     */
    void publish(const std::string& pluginName, 
                const std::string& topic, 
                const std::string& data = "") {
        Message msg = Message::create(topic, data).from(pluginName);
        MessageBus::getInstance().publish(msg);
    }
    
    // ========== 资源管理集成 ==========
    
    /**
     * @brief 注册插件资源
     */
    uint64_t registerResource(const std::string& pluginName,
                             const std::string& name,
                             ResourceType type,
                             void* handle,
                             size_t size = 0) {
        uint64_t resId = ResourceManager::getInstance().registerResource(name, type, handle, pluginName, size);
        extendedInfo_[pluginName].resources.push_back(resId);
        return resId;
    }
    
    // ========== 配置系统集成 ==========
    
    /**
     * @brief 获取插件配置
     */
    ConfigValue getPluginConfig(const std::string& pluginName, 
                               const std::string& key,
                               const ConfigValue& def = ConfigValue()) {
        return PluginConfigManager::getInstance().getPluginConfig(pluginName, key, def);
    }
    
    /**
     * @brief 设置插件配置
     */
    void setPluginConfig(const std::string& pluginName,
                        const std::string& key,
                        const ConfigValue& value) {
        PluginConfigManager::getInstance().setPluginConfig(pluginName, key, value);
    }
    
    // ========== 查询 ==========
    
    IPlugin* getPlugin(const std::string& name) const {
        return basicLoader_.getPlugin(name);
    }
    
    PluginState getState(const std::string& name) const {
        auto it = extendedInfo_.find(name);
        return (it != extendedInfo_.end()) ? it->second.state : PluginState::UNLOADED;
    }
    
    ExtendedPluginInfo getExtendedInfo(const std::string& name) const {
        auto it = extendedInfo_.find(name);
        return (it != extendedInfo_.end()) ? it->second : ExtendedPluginInfo();
    }
    
    std::vector<std::string> getLoadedPlugins() const {
        return basicLoader_.getLoadedPlugins();
    }
    
    std::vector<std::string> getPluginsByState(PluginState state) const {
        std::vector<std::string> result;
        for (const auto& pair : extendedInfo_) {
            if (pair.second.state == state) {
                result.push_back(pair.first);
            }
        }
        return result;
    }
    
    // ========== 执行 ==========
    
    PluginResult execute(const std::string& name, const PluginParams& params) {
        return basicLoader_.execute(name, params);
    }
    
    // ========== 事件 ==========
    
    void setEventCallback(PluginEventCallback callback) {
        eventCallback_ = callback;
    }
    
    // ========== 统计 ==========
    
    size_t getLoadedCount() const {
        return basicLoader_.getLoadedCount();
    }

private:
    PluginLoader basicLoader_;
    AdvancedLoaderConfig config_;
    DependencyResolver resolver_;
    std::map<std::string, ExtendedPluginInfo> extendedInfo_;
    PluginEventCallback eventCallback_;
    
    void fireEvent(const std::string& pluginName, const std::string& event, const std::string& data) {
        if (eventCallback_) {
            eventCallback_(pluginName, event, data);
        }
        
        if (config_.enableMessageBus) {
            MessageBus::getInstance().publish(
                MessageTopics::PLUGIN_LOADED,
                pluginName + ":" + event
            );
        }
    }
    
    void loadPluginConfig(const std::string& pluginName) {
        if (!config_.configFilePath.empty()) {
            PluginConfigManager::getInstance().loadFromFile(config_.configFilePath);
        }
    }
    
    void unsubscribePlugin(const std::string& pluginName) {
        MessageBus::getInstance().unsubscribeAll(pluginName);
    }
    
    void cleanupPluginResources(const std::string& pluginName) {
        ResourceManager::getInstance().releaseOwnerResources(pluginName);
    }
    
    void cleanupAllResources() {
        for (const auto& pair : extendedInfo_) {
            ResourceManager::getInstance().releaseOwnerResources(pair.first);
        }
    }
    
    int64_t getCurrentTime() const {
        return std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_ADVANCED_PLUGIN_LOADER_H
