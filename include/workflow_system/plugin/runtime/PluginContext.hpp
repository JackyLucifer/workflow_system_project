#ifndef PLUGIN_FRAMEWORK_PLUGIN_CONTEXT_HPP
#define PLUGIN_FRAMEWORK_PLUGIN_CONTEXT_HPP

#include "workflow_system/plugin/core/IPluginContext.hpp"
#include "workflow_system/plugin/communication/Channel.hpp"
#include "workflow_system/core/logger.h"
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <vector>

namespace WorkflowSystem { namespace Plugin {

// 前向声明
class ThreadPool;
class TimerService;
class IPluginManager;
class IEventBus;
class IMessageBus;
// 使用工作流系统的 Logger，不再需要前向声明
class IDataChannel;

/**
 * @brief 插件上下文实现
 */
class PluginContext : public IPluginContext {
public:
    PluginContext(const std::string& pluginId,
                  const PluginSpec& spec,
                  IPluginManager* manager,
                  IEventBus* eventBus,
                  IMessageBus* messageBus);

    ~PluginContext() override;

    // ==================== 插件信息 ====================
    std::string getPluginId() const override;
    PluginSpec getSpec() const override;
    PluginState getState() const override;
    std::string getPluginDirectory() const override;
    std::string getDataDirectory() const override;
    std::string getConfigDirectory() const override;
    std::string getLogDirectory() const override;

    // ==================== 配置管理 ====================
    std::string getConfig(const std::string& key) const override;
    void setConfig(const std::string& key, const std::string& value) override;
    std::string getAllConfig() const override;
    bool saveConfig() override;
    bool reloadConfig() override;

    // ==================== 日志系统 ====================
    WorkflowSystem::Logger& getLogger() override;
    void logDebug(const std::string& message) override;
    void logInfo(const std::string& message) override;
    void logWarning(const std::string& message) override;
    void logError(const std::string& message) override;

    // ==================== 事件系统 ====================
    SubscriptionId subscribe(const std::string& event, EventHandler handler) override;
    SubscriptionId subscribe(const std::string& event, EventHandler handler, EventPriority priority) override;
    void unsubscribe(SubscriptionId subscriptionId) override;
    void unsubscribeAll() override;
    EventResult publish(const Event& event) override;
    void publishAsync(const Event& event) override;
    EventResult publish(const std::string& eventName, const EventData& data) override;
    void publishAsync(const std::string& eventName, const EventData& data) override;

    // ==================== 消息系统 ====================
    Response sendMessage(const std::string& to, const Message& message, Duration timeout) override;
    void sendNotification(const std::string& to, const Message& message) override;
    void broadcast(const Message& message) override;
    std::string callRpc(const std::string& pluginId, const std::string& method, 
                       const std::string& params, Duration timeout) override;

    // ==================== 插件发现 ====================
    IPluginManager* getPluginManager() override;
    IPlugin* getPlugin(const std::string& pluginId) const override;
    std::vector<IPlugin*> getAllPlugins() const override;
    bool hasPlugin(const std::string& pluginId) const override;
    bool isPluginLoaded(const std::string& pluginId) const override;

    // ==================== 数据通道 ====================
    IDataChannel* createDataChannel(const std::string& channelId) override;
    IDataChannel* getDataChannel(const std::string& channelId) override;
    void closeDataChannel(const std::string& channelId) override;

    // ==================== 资源管理 ====================
    void registerResource(const std::string& resourceId, void* resource, 
                         std::function<void(void*)> deleter) override;
    void* getResource(const std::string& resourceId) const override;
    void unregisterResource(const std::string& resourceId) override;

    // ==================== 定时器 ====================
    TimerId scheduleDelayed(Duration delay, Callback callback) override;
    TimerId scheduleInterval(Duration interval, Callback callback) override;
    void cancelTimer(TimerId timerId) override;

    // ==================== 线程池 ====================
    void executeAsync(Callback task) override;
    void executeAsync(Callback task, Callback onComplete) override;
    ThreadPool* getThreadPool() override;

    // ==================== 生命周期控制 ====================
    bool requestRestart() override;
    bool requestReloadConfig() override;
    bool requestDisable() override;

    // ==================== 通讯通道 ====================
    std::shared_ptr<Channel> getChannel(const std::string& name) override;
    std::shared_ptr<Channel> getDefaultChannel() override;
    SubscriptionId subscribeData(const std::string& topic, DataCallback callback) override;
    void unsubscribeData(SubscriptionId id) override;
    void publishData(const std::string& topic, 
                    const std::map<std::string, std::string>& data) override;
    void publishDataSync(const std::string& topic,
                        const std::map<std::string, std::string>& data) override;

    // ==================== 系统服务 ====================
    void* getService(const std::string& serviceName) const override;
    void registerService(const std::string& serviceName, void* service) override;

    // ==================== 内部方法 ====================
    void setState(PluginState state);
    void initialize();
    void shutdown();

private:
    std::string pluginId_;
    PluginSpec spec_;
    PluginState state_;
    IPluginManager* manager_;
    IEventBus* eventBus_;
    IMessageBus* messageBus_;

    std::string pluginDirectory_;
    std::string dataDirectory_;
    std::string configDirectory_;
    std::string logDirectory_;

    std::map<std::string, std::string> config_;
    mutable std::mutex configMutex_;

    WorkflowSystem::Logger* logger_;

    std::vector<SubscriptionId> subscriptions_;
    mutable std::mutex subscriptionsMutex_;

    std::map<std::string, IDataChannel*> dataChannels_;
    mutable std::mutex dataChannelsMutex_;

    struct ResourceEntry {
        void* resource;
        std::function<void(void*)> deleter;
    };
    std::map<std::string, ResourceEntry> resources_;
    mutable std::mutex resourcesMutex_;

    std::map<TimerId, void*> timers_;
    mutable std::mutex timersMutex_;
    TimerId nextTimerId_;

    std::unique_ptr<ThreadPool> threadPool_;

    std::map<std::string, void*> services_;
    mutable std::mutex servicesMutex_;

    std::shared_ptr<Channel> defaultChannel_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_PLUGIN_CONTEXT_HPP
