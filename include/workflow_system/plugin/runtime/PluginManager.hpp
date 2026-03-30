#ifndef PLUGIN_FRAMEWORK_PLUGIN_MANAGER_HPP
#define PLUGIN_FRAMEWORK_PLUGIN_MANAGER_HPP

#include "workflow_system/plugin/core/IPluginManager.hpp"
#include "workflow_system/plugin/core/IPlugin.hpp"
#include "workflow_system/plugin/runtime/PluginContext.hpp"
#include "workflow_system/plugin/communication/EventBus.hpp"
#include "workflow_system/plugin/communication/MessageBus.hpp"
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 插件条目
 */
struct PluginEntry {
    std::string pluginId;
    PluginSpec spec;
    std::shared_ptr<IPlugin> plugin;
    std::unique_ptr<PluginContext> context;
    std::string libraryPath;
    void* libraryHandle = nullptr;
    PluginState state = PluginState::UNLOADED;

    // 使用IPluginManager中定义的工厂函数类型
    using CreateFunc = IPlugin* (*)();
    using DestroyFunc = void (*)(IPlugin*);

    CreateFunc createFunc = nullptr;
    DestroyFunc destroyFunc = nullptr;

    ~PluginEntry() {
        if (destroyFunc && plugin) {
            destroyFunc(plugin.get());
        }
    }
};

/**
 * @brief 插件管理器实现
 */
class PluginManager : public IPluginManager {
public:
    PluginManager();
    explicit PluginManager(const std::string& pluginDirectory);
    ~PluginManager() override;

    // ==================== 插件加载 ====================

    LoadResult loadPlugin(const std::string& pluginPath) override;
    LoadResult loadPlugin(const PluginSpec& spec,
                         const std::string& libraryPath) override;
    int loadPluginsFromDirectory(const std::string& directory,
                                bool recursive = false) override;
    int loadPluginsFromConfig(const std::string& configFile) override;

    // ==================== 插件卸载 ====================

    UnloadResult unloadPlugin(const std::string& pluginId) override;
    int unloadAllPlugins() override;
    UnloadResult forceUnloadPlugin(const std::string& pluginId) override;

    // ==================== 插件重载 ====================

    LoadResult reloadPlugin(const std::string& pluginId) override;
    bool hotReloadPlugin(const std::string& pluginId) override;

    // ==================== 插件启用/禁用 ====================

    bool enablePlugin(const std::string& pluginId) override;
    bool disablePlugin(const std::string& pluginId) override;

    // ==================== 插件查询 ====================

    IPlugin* getPlugin(const std::string& pluginId) override;
    const IPlugin* getPlugin(const std::string& pluginId) const override;
    std::vector<IPlugin*> getAllPlugins() override;
    std::vector<const IPlugin*> getAllPlugins() const override;
    std::vector<PluginSpec> getLoadedPluginSpecs() const override;
    PluginInstanceInfo getPluginInfo(const std::string& pluginId) const override;
    std::vector<PluginInstanceInfo> getAllPluginInfo() const override;

    // ==================== 插件状态 ====================

    bool isPluginLoaded(const std::string& pluginId) const override;
    bool isPluginRunning(const std::string& pluginId) const override;
    PluginState getPluginState(const std::string& pluginId) const override;

    // ==================== 插件调用 ====================

    std::string callPlugin(const std::string& pluginId,
                          const std::string& method,
                          const std::string& params) override;
    Response sendPluginMessage(const std::string& pluginId,
                              const Message& message) override;

    // ==================== 批量操作 ====================

    int startAllPlugins() override;
    int stopAllPlugins() override;
    int pauseAllPlugins() override;
    int resumeAllPlugins() override;

    // ==================== 依赖管理 ====================

    bool resolveDependencies(const std::string& pluginId) override;
    std::map<std::string, std::vector<std::string>> getDependencyGraph() const override;
    std::vector<std::string> getLoadOrder() const override;

    // ==================== 系统信息 ====================

    PluginManagerStatistics getStatistics() const override;
    std::string getSystemStatus() const override;
    std::string getHealthReport() const override;

    // ==================== 事件和消息 ====================

    EventResult broadcastEvent(const Event& event) override;
    void broadcastMessage(const Message& message) override;

    // ==================== 配置 ====================

    std::string getPluginDirectory() const override;
    void setPluginDirectory(const std::string& directory) override;
    std::string getConfigDirectory() const override;
    void setConfigDirectory(const std::string& directory) override;
    std::string getDataDirectory() const override;
    void setDataDirectory(const std::string& directory) override;

    // ==================== 热加载 ====================

    bool enableHotReload(const std::string& directory) override;
    void disableHotReload() override;
    bool isHotReloadEnabled() const override;

    // ==================== 额外功能 ====================

    /**
     * @brief 初始化管理器
     */
    bool initialize();

    /**
     * @brief 关闭管理器
     */
    void shutdown();

    /**
     * @brief 获取事件总线
     */
    IEventBus* getEventBus();

    /**
     * @brief 获取消息总线
     */
    IMessageBus* getMessageBus();

private:
    // 内部方法
    LoadResult loadPluginInternal(const std::string& pluginPath,
                                 const PluginSpec* externalSpec = nullptr);
    UnloadResult unloadPluginInternal(const std::string& pluginId, bool force);
    bool transitionState(PluginEntry& entry, PluginState newState);
    bool checkDependencies(const PluginSpec& spec);
    std::vector<std::string> calculateLoadOrder(const std::string& pluginId);
    void cleanupPluginEntry(PluginEntry& entry);

    // 插件存储
    std::map<std::string, std::unique_ptr<PluginEntry>> plugins_;
    mutable std::mutex pluginsMutex_;

    // 目录配置
    std::string pluginDirectory_;
    std::string configDirectory_;
    std::string dataDirectory_;

    // 通信
    std::unique_ptr<EventBus> eventBus_;
    std::unique_ptr<MessageBus> messageBus_;

    // 热加载
    std::atomic<bool> hotReloadEnabled_{false};
    std::thread hotReloadThread_;

    // 统计
    mutable std::mutex statsMutex_;
    PluginManagerStatistics statistics_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_PLUGIN_MANAGER_HPP
