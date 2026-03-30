#ifndef PLUGIN_FRAMEWORK_IPLUGIN_MANAGER_HPP
#define PLUGIN_FRAMEWORK_IPLUGIN_MANAGER_HPP

#include "PluginInfo.hpp"
#include "IPlugin.hpp"
#include "IPluginContext.hpp"
#include <string>
#include <vector>
#include <memory>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 插件加载结果
 */
struct LoadResult {
    bool success;
    std::string pluginId;
    std::string message;
    PluginState state;

    static LoadResult ok(const std::string& id) {
        return {true, id, "Loaded successfully", PluginState::LOADED};
    }

    static LoadResult error(const std::string& msg) {
        return {false, "", msg, PluginState::ERROR};
    }
};

/**
 * @brief 插件卸载结果
 */
struct UnloadResult {
    bool success;
    std::string message;

    static UnloadResult ok() {
        return {true, "Unloaded successfully"};
    }

    static UnloadResult error(const std::string& msg) {
        return {false, msg};
    }
};

/**
 * @brief 插件管理器统计信息
 */
struct PluginManagerStatistics {
    size_t totalPlugins = 0;
    size_t loadedPlugins = 0;
    size_t runningPlugins = 0;
    size_t errorPlugins = 0;
    size_t totalMessages = 0;
    size_t totalEvents = 0;
};

/**
 * @brief 插件管理器接口
 *
 * 管理插件的生命周期、依赖关系和通信
 */
class IPluginManager {
public:
    virtual ~IPluginManager() = default;

    // ==================== 插件加载 ====================

    /**
     * @brief 加载插件
     * @param pluginPath 插件库文件路径
     * @return 加载结果
     */
    virtual LoadResult loadPlugin(const std::string& pluginPath) = 0;

    /**
     * @brief 加载插件（从规范）
     * @param spec 插件规范
     * @param libraryPath 库文件路径
     * @return 加载结果
     */
    virtual LoadResult loadPlugin(const PluginSpec& spec,
                                const std::string& libraryPath) = 0;

    /**
     * @brief 从目录加载所有插件
     * @param directory 目录路径
     * @param recursive 是否递归搜索子目录
     * @return 成功加载的插件数量
     */
    virtual int loadPluginsFromDirectory(const std::string& directory,
                                        bool recursive = false) = 0;

    /**
     * @brief 从配置文件加载插件
     * @param configFile 配置文件路径
     * @return 加载的插件数量
     */
    virtual int loadPluginsFromConfig(const std::string& configFile) = 0;

    // ==================== 插件卸载 ====================

    /**
     * @brief 卸载插件
     * @param pluginId 插件ID
     * @return 卸载结果
     */
    virtual UnloadResult unloadPlugin(const std::string& pluginId) = 0;

    /**
     * @brief 卸载所有插件
     * @return 卸载的插件数量
     */
    virtual int unloadAllPlugins() = 0;

    /**
     * @brief 强制卸载插件（忽略依赖）
     * @param pluginId 插件ID
     * @return 卸载结果
     */
    virtual UnloadResult forceUnloadPlugin(const std::string& pluginId) = 0;

    // ==================== 插件重载 ====================

    /**
     * @brief 重新加载插件
     * @param pluginId 插件ID
     * @return 加载结果
     */
    virtual LoadResult reloadPlugin(const std::string& pluginId) = 0;

    /**
     * @brief 热重载插件（不停止运行）
     * @param pluginId 插件ID
     * @return 成功返回true
     */
    virtual bool hotReloadPlugin(const std::string& pluginId) = 0;

    // ==================== 插件启用/禁用 ====================

    /**
     * @brief 启用插件
     * @param pluginId 插件ID
     * @return 成功返回true
     */
    virtual bool enablePlugin(const std::string& pluginId) = 0;

    /**
     * @brief 禁用插件
     * @param pluginId 插件ID
     * @return 成功返回true
     */
    virtual bool disablePlugin(const std::string& pluginId) = 0;

    // ==================== 插件查询 ====================

    /**
     * @brief 获取插件
     * @param pluginId 插件ID
     * @return 插件指针（不存在返回nullptr）
     */
    virtual IPlugin* getPlugin(const std::string& pluginId) = 0;

    /**
     * @brief 获取插件（常量版本）
     */
    virtual const IPlugin* getPlugin(const std::string& pluginId) const = 0;

    /**
     * @brief 获取所有插件
     * @return 插件指针列表
     */
    virtual std::vector<IPlugin*> getAllPlugins() = 0;

    /**
     * @brief 获取所有插件（常量版本）
     */
    virtual std::vector<const IPlugin*> getAllPlugins() const = 0;

    /**
     * @brief 获取已加载的插件规范列表
     * @return 插件规范列表
     */
    virtual std::vector<PluginSpec> getLoadedPluginSpecs() const = 0;

    /**
     * @brief 获取插件实例信息
     * @param pluginId 插件ID
     * @return 插件实例信息
     */
    virtual PluginInstanceInfo getPluginInfo(const std::string& pluginId) const = 0;

    /**
     * @brief 获取所有插件实例信息
     * @return 插件实例信息列表
     */
    virtual std::vector<PluginInstanceInfo> getAllPluginInfo() const = 0;

    // ==================== 插件状态 ====================

    /**
     * @brief 检查插件是否已加载
     * @param pluginId 插件ID
     * @return 已加载返回true
     */
    virtual bool isPluginLoaded(const std::string& pluginId) const = 0;

    /**
     * @brief 检查插件是否正在运行
     * @param pluginId 插件ID
     * @return 运行中返回true
     */
    virtual bool isPluginRunning(const std::string& pluginId) const = 0;

    /**
     * @brief 获取插件状态
     * @param pluginId 插件ID
     * @return 插件状态
     */
    virtual PluginState getPluginState(const std::string& pluginId) const = 0;

    // ==================== 插件调用 ====================

    /**
     * @brief 调用插件方法
     * @param pluginId 插件ID
     * @param method 方法名
     * @param params 参数（JSON字符串）
     * @return 返回值（JSON字符串）
     */
    virtual std::string callPlugin(const std::string& pluginId,
                                 const std::string& method,
                                 const std::string& params = "{}") = 0;

    /**
     * @brief 向插件发送消息
     * @param pluginId 插件ID
     * @param message 消息对象
     * @return 响应
     */
    virtual Response sendPluginMessage(const std::string& pluginId,
                                     const Message& message) = 0;

    // ==================== 批量操作 ====================

    /**
     * @brief 启动所有插件
     * @return 成功启动的插件数量
     */
    virtual int startAllPlugins() = 0;

    /**
     * @brief 停止所有插件
     * @return 成功停止的插件数量
     */
    virtual int stopAllPlugins() = 0;

    /**
     * @brief 暂停所有插件
     * @return 成功暂停的插件数量
     */
    virtual int pauseAllPlugins() = 0;

    /**
     * @brief 恢复所有插件
     * @return 成功恢复的插件数量
     */
    virtual int resumeAllPlugins() = 0;

    // ==================== 依赖管理 ====================

    /**
     * @brief 解析插件依赖
     * @param pluginId 插件ID
     * @return 成功返回true
     */
    virtual bool resolveDependencies(const std::string& pluginId) = 0;

    /**
     * @brief 获取依赖图
     * @return 依赖关系（pluginId -> 依赖列表）
     */
    virtual std::map<std::string, std::vector<std::string>>
    getDependencyGraph() const = 0;

    /**
     * @brief 获取加载顺序
     * @return 按加载顺序排列的插件ID列表
     */
    virtual std::vector<std::string> getLoadOrder() const = 0;

    // ==================== 系统信息 ====================

    /**
     * @brief 获取统计信息
     */
    virtual PluginManagerStatistics getStatistics() const = 0;

    /**
     * @brief 获取系统状态
     */
    virtual std::string getSystemStatus() const = 0;

    /**
     * @brief 获取健康报告
     */
    virtual std::string getHealthReport() const = 0;

    // ==================== 事件和消息 ====================

    /**
     * @brief 广播事件到所有插件
     * @param event 事件对象
     * @return 事件结果
     */
    virtual EventResult broadcastEvent(const Event& event) = 0;

    /**
     * @brief 广播消息到所有插件
     * @param message 消息对象
     */
    virtual void broadcastMessage(const Message& message) = 0;

    // ==================== 配置 ====================

    /**
     * @brief 获取插件目录
     */
    virtual std::string getPluginDirectory() const = 0;

    /**
     * @brief 设置插件目录
     */
    virtual void setPluginDirectory(const std::string& directory) = 0;

    /**
     * @brief 获取配置目录
     */
    virtual std::string getConfigDirectory() const = 0;

    /**
     * @brief 设置配置目录
     */
    virtual void setConfigDirectory(const std::string& directory) = 0;

    /**
     * @brief 获取数据目录
     */
    virtual std::string getDataDirectory() const = 0;

    /**
     * @brief 设置数据目录
     */
    virtual void setDataDirectory(const std::string& directory) = 0;

    // ==================== 热加载 ====================

    /**
     * @brief 启用热加载监控
     * @param directory 监控目录
     * @return 成功返回true
     */
    virtual bool enableHotReload(const std::string& directory) = 0;

    /**
     * @brief 禁用热加载监控
     */
    virtual void disableHotReload() = 0;

    /**
     * @brief 检查热加载是否启用
     */
    virtual bool isHotReloadEnabled() const = 0;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_IPLUGIN_MANAGER_HPP
