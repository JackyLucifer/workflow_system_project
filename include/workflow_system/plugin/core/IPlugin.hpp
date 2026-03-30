#ifndef PLUGIN_FRAMEWORK_IPLUGIN_HPP
#define PLUGIN_FRAMEWORK_IPLUGIN_HPP

#include "PluginInfo.hpp"
#include "../communication/Event.hpp"
#include "../communication/Message.hpp"
#include <memory>
#include <vector>
#include <string>
#include <sstream>

namespace WorkflowSystem { namespace Plugin {

class IPluginContext;
class IPluginManager;

/**
 * @brief 插件接口
 *
 * 所有插件都必须实现此接口
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // ==================== 基本信息 ====================

    /**
     * @brief 获取插件规范
     * @return 插件规范（静态信息）
     */
    virtual PluginSpec getSpec() const = 0;

    /**
     * @brief 获取插件实例信息
     * @return 插件实例信息（运行时信息）
     */
    virtual PluginInstanceInfo getInstanceInfo() const {
        PluginInstanceInfo info;
        info.spec = getSpec();
        return info;
    }

    // ==================== 生命周期 ====================

    /**
     * @brief 插件加载时调用
     */
    virtual bool onLoad(IPluginContext* context) = 0;

    /**
     * @brief 插件卸载时调用
     */
    virtual void onUnload() = 0;

    /**
     * @brief 插件初始化
     */
    virtual bool onInitialize() = 0;

    /**
     * @brief 插件启动
     */
    virtual bool onStart() = 0;

    /**
     * @brief 插件停止
     */
    virtual void onStop() {}

    /**
     * @brief 插件清理
     */
    virtual void onCleanup() {}

    // ==================== 配置管理 ====================

    /**
     * @brief 加载配置
     */
    virtual bool loadConfig(const std::string& config) {
        return true;
    }

    /**
     * @brief 获取当前配置
     */
    virtual std::string getConfig() const {
        return "{}";
    }

    /**
     * @brief 配置变更回调
     */
    virtual void onConfigChanged(const std::string& key) {}

    /**
     * @brief 验证配置
     */
    virtual bool validateConfig(const std::string& config) const {
        return true;
    }

    // ==================== 依赖管理 ====================

    /**
     * @brief 获取依赖列表
     */
    virtual std::vector<Dependency> getDependencies() const {
        return {};
    }

    /**
     * @brief 依赖插件加载完成回调
     */
    virtual bool onDependencyLoaded(const std::string& pluginId) {
        return true;
    }

    /**
     * @brief 依赖插件卸载回调
     */
    virtual void onDependencyUnloaded(const std::string& pluginId) {}

    /**
     * @brief 检查依赖是否满足
     */
    virtual bool checkDependencies() const {
        return true;
    }

    // ==================== 通信接口 ====================

    /**
     * @brief 接收消息
     */
    virtual Response onMessage(const Message& message) = 0;

    /**
     * @brief 接收事件
     */
    virtual void onEvent(const IEvent& event) {}

    /**
     * @brief 接收数据
     */
    virtual void onDataReceived(const std::vector<uint8_t>& data) {}

    // ==================== RPC接口 ====================

    /**
     * @brief RPC方法调用
     */
    virtual std::string callMethod(const std::string& method,
                                   const std::string& params) {
        return "{\"error\": \"Method not implemented: " + method + "\"}";
    }

    /**
     * @brief 获取支持的RPC方法列表
     */
    virtual std::vector<std::string> getSupportedMethods() const {
        return {};
    }

    // ==================== 热重载支持 ====================

    /**
     * @brief 准备数据迁移
     */
    virtual std::string prepareMigration() const {
        return "{}";
    }

    /**
     * @brief 从旧版本迁移数据
     */
    virtual bool migrateData(const Version& oldVersion,
                            const std::string& migrationData) {
        return true;
    }

    /**
     * @brief 插件重载回调
     */
    virtual bool onReload(const Version& oldVersion) {
        return true;
    }

    // ==================== 健康与监控 ====================

    /**
     * @brief 获取健康状态
     */
    virtual HealthStatus getHealthStatus() const {
        HealthStatus hs;
        hs.status = HealthStatus::HEALTHY;
        hs.message = "OK";
        return hs;
    }

    /**
     * @brief 获取性能指标
     */
    virtual PluginMetrics getMetrics() const {
        return PluginMetrics{};
    }

    /**
     * @brief 执行健康检查
     */
    virtual bool healthCheck() const {
        return true;
    }

    // ==================== 调试与诊断 ====================

    /**
     * @brief 获取调试信息
     */
    virtual std::string getDebugInfo() const {
        return "No debug info available";
    }

    /**
     * @brief 获取诊断信息
     */
    virtual std::string getDiagnostics() const {
        return "{}";
    }

    // ==================== 状态查询 ====================

    /**
     * @brief 获取插件状态
     */
    virtual std::string getStatus() const {
        return "Running";
    }

    /**
     * @brief 获取插件详细状态
     */
    virtual std::string getStatusDetails() const {
        PluginInstanceInfo info = getInstanceInfo();
        std::ostringstream oss;
        oss << "Plugin: " << info.spec.name
            << " (v" << info.spec.version.toString() << ")\n"
            << "State: " << stateToString(info.state)
            << "\nHealth: " << (info.healthStatus.isHealthy() ? "OK" : "Not OK");
        return oss.str();
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_IPLUGIN_HPP
