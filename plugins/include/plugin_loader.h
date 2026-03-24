/**
 * @file plugin_loader.h
 * @brief 插件加载器 - 动态加载和管理插件
 */

#ifndef WORKFLOW_SYSTEM_PLUGIN_LOADER_H
#define WORKFLOW_SYSTEM_PLUGIN_LOADER_H

#include "plugin_interface.h"
#include <mutex>
#include <map>
#include <vector>
#include <functional>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 插件加载器配置
 */
struct PluginLoaderConfig {
    std::vector<std::string> searchPaths;       // 插件搜索路径
    std::string fileExtension;                   // 插件文件扩展名
    bool autoLoadDependencies;                   // 自动加载依赖
    bool enableHotReload;                        // 启用热重载
    bool strictVersionCheck;                     // 严格版本检查
    
    PluginLoaderConfig()
        : fileExtension(
#ifdef _WIN32
            ".dll"
#else
            ".so"
#endif
          )
        , autoLoadDependencies(true)
        , enableHotReload(false)
        , strictVersionCheck(false) {}
};

/**
 * @brief 加载的插件信息
 */
struct LoadedPlugin {
    std::string path;                            // 插件路径
    void* handle;                                // 动态库句柄
    IPlugin* instance;                           // 插件实例
    CreatePluginFunc createFunc;                 // 创建函数
    DestroyPluginFunc destroyFunc;               // 销毁函数
    PluginMetadata metadata;                     // 元数据
    PluginState state;                           // 状态
    int64_t loadTime;                            // 加载时间
    
    LoadedPlugin()
        : handle(nullptr)
        , instance(nullptr)
        , createFunc(nullptr)
        , destroyFunc(nullptr)
        , state(PluginState::UNLOADED)
        , loadTime(0) {}
};

/**
 * @brief 插件加载器
 */
class PluginLoader {
public:
    using EventCallback = std::function<void(const std::string& pluginName, const std::string& event)>;
    
    PluginLoader();
    explicit PluginLoader(const PluginLoaderConfig& config);
    ~PluginLoader();
    
    // 禁止拷贝
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    
    // ========== 配置 ==========
    
    void setConfig(const PluginLoaderConfig& config);
    PluginLoaderConfig getConfig() const;
    
    void addSearchPath(const std::string& path);
    void clearSearchPaths();
    
    // ========== 插件发现 ==========
    
    /**
     * @brief 扫描插件目录
     * @return 发现的插件路径列表
     */
    std::vector<std::string> scanPlugins();
    
    /**
     * @brief 检查插件是否存在
     */
    bool hasPlugin(const std::string& name) const;
    
    /**
     * @brief 获取插件路径
     */
    std::string getPluginPath(const std::string& name) const;
    
    // ========== 插件加载 ==========
    
    /**
     * @brief 加载插件
     * @param path 插件路径或名称
     * @return 是否成功
     */
    bool load(const std::string& path);
    
    /**
     * @brief 加载所有发现的插件
     * @return 成功加载的数量
     */
    int loadAll();
    
    /**
     * @brief 卸载插件
     */
    bool unload(const std::string& name);
    
    /**
     * @brief 卸载所有插件
     */
    void unloadAll();
    
    /**
     * @brief 重新加载插件
     */
    bool reload(const std::string& name);
    
    // ========== 插件生命周期 ==========
    
    /**
     * @brief 初始化插件
     */
    bool initialize(const std::string& name, IPluginContext* context = nullptr);
    
    /**
     * @brief 初始化所有已加载插件
     */
    int initializeAll(IPluginContext* context = nullptr);
    
    /**
     * @brief 启动插件
     */
    bool start(const std::string& name);
    
    /**
     * @brief 启动所有已初始化插件
     */
    int startAll();
    
    /**
     * @brief 停止插件
     */
    bool stop(const std::string& name);
    
    /**
     * @brief 停止所有运行中插件
     */
    int stopAll();
    
    // ========== 插件访问 ==========
    
    /**
     * @brief 获取插件实例
     */
    IPlugin* getPlugin(const std::string& name) const;
    
    /**
     * @brief 获取插件元数据
     */
    PluginMetadata getMetadata(const std::string& name) const;
    
    /**
     * @brief 获取插件状态
     */
    PluginState getState(const std::string& name) const;
    
    /**
     * @brief 获取所有已加载插件名称
     */
    std::vector<std::string> getLoadedPlugins() const;
    
    /**
     * @brief 获取指定状态的插件
     */
    std::vector<std::string> getPluginsByState(PluginState state) const;
    
    // ========== 插件执行 ==========
    
    /**
     * @brief 执行插件
     */
    PluginResult execute(const std::string& name, const PluginParams& params);
    
    /**
     * @brief 执行所有支持某动作的插件
     */
    std::vector<std::pair<std::string, PluginResult>> executeAll(
        const std::string& action, 
        const PluginParams& params);
    
    // ========== 事件 ==========
    
    void setEventCallback(EventCallback callback);
    
    // ========== 统计 ==========
    
    size_t getLoadedCount() const;
    int64_t getTotalLoadTime() const;
    
private:
    mutable std::mutex mutex_;
    PluginLoaderConfig config_;
    std::map<std::string, LoadedPlugin> plugins_;
    IPluginContext* defaultContext_;
    EventCallback eventCallback_;
    
    bool loadLibrary(const std::string& path, LoadedPlugin& info);
    void unloadLibrary(LoadedPlugin& info);
    std::string resolvePath(const std::string& nameOrPath) const;
    void fireEvent(const std::string& pluginName, const std::string& event);
    
    // 默认插件上下文实现
    class DefaultPluginContext : public IPluginContext {
    public:
        std::string logBuffer;
        std::map<std::string, std::string> config;
        std::map<std::string, void*> services;
        
        void logInfo(const std::string& message) override {
            logBuffer += "[INFO] " + message + "\n";
        }
        void logWarning(const std::string& message) override {
            logBuffer += "[WARN] " + message + "\n";
        }
        void logError(const std::string& message) override {
            logBuffer += "[ERROR] " + message + "\n";
        }
        void logDebug(const std::string& message) override {
            logBuffer += "[DEBUG] " + message + "\n";
        }
        
        std::string getConfig(const std::string& key, const std::string& defaultValue = "") const override {
            auto it = config.find(key);
            return (it != config.end()) ? it->second : defaultValue;
        }
        void setConfig(const std::string& key, const std::string& value) override {
            config[key] = value;
        }
        
        bool registerService(const std::string& serviceName, void* service) override {
            services[serviceName] = service;
            return true;
        }
        void* getService(const std::string& serviceName) const override {
            auto it = services.find(serviceName);
            return (it != services.end()) ? it->second : nullptr;
        }
        bool hasService(const std::string& serviceName) const override {
            return services.find(serviceName) != services.end();
        }
        
        std::string getPluginPath() const override { return ""; }
        std::string getDataPath() const override { return ""; }
        std::string getTempPath() const override { return ""; }
        
        void emitEvent(const std::string& eventName, const std::string& eventData) override {
            (void)eventName;
            (void)eventData;
        }
        void subscribeEvent(const std::string& eventName,
                           std::function<void(const std::string&)> callback) override {
            (void)eventName;
            (void)callback;
        }
    };
    
    DefaultPluginContext defaultContextImpl_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_PLUGIN_LOADER_H
