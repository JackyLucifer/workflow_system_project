#ifndef PLUGIN_FRAMEWORK_IPLUGIN_CONTEXT_HPP
#define PLUGIN_FRAMEWORK_IPLUGIN_CONTEXT_HPP

#include "PluginInfo.hpp"
#include "IPlugin.hpp"
#include "../communication/Event.hpp"
#include "../communication/Message.hpp"
#include "../communication/Channel.hpp"
#include "workflow_system/core/logger.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace WorkflowSystem { namespace Plugin {

// 前向声明
class IPluginManager;
// 使用工作流系统的 Logger，不再需要前向声明
class TimerService;
class ThreadPool;

/**
 * @brief 回调函数类型
 */
using Callback = std::function<void()>;

/**
 * @brief 定时器ID
 */
using TimerId = uint64_t;

/**
 * @brief 时间间隔
 */
class Duration {
public:
    uint64_t ms = 0;

    static Duration fromMilliseconds(uint64_t milliseconds) {
        Duration d; d.ms = milliseconds; return d;
    }

    static Duration fromSeconds(uint64_t seconds) {
        return fromMilliseconds(seconds * 1000);
    }

    static Duration fromMinutes(uint64_t minutes) {
        return fromSeconds(minutes * 60);
    }

    static Duration fromHours(uint64_t hours) {
        return fromMinutes(hours * 60);
    }
    
    uint64_t getMilliseconds() const { return ms; }
};

/**
 * @brief 数据通道接口
 */
class IDataChannel {
public:
    virtual ~IDataChannel() = default;

    /**
     * @brief 发送数据
     */
    virtual void send(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief 接收数据
     */
    virtual std::vector<uint8_t> receive() = 0;

    /**
     * @brief 接收数据（带超时）
     */
    virtual std::vector<std::vector<uint8_t>> receiveBatch(size_t count, Duration timeout) = 0;

    /**
     * @brief 获取通道ID
     */
    virtual std::string getId() const = 0;

    /**
     * @brief 关闭通道
     */
    virtual void close() = 0;
};

/**
 * @brief 插件上下文接口
 *
 * 提供插件与框架交互的所有功能
 */
class IPluginContext {
public:
    virtual ~IPluginContext() = default;

    // ==================== 插件信息 ====================

    /**
     * @brief 获取插件ID
     */
    virtual std::string getPluginId() const = 0;

    /**
     * @brief 获取插件规范
     */
    virtual PluginSpec getSpec() const = 0;

    /**
     * @brief 获取当前状态
     */
    virtual PluginState getState() const = 0;

    /**
     * @brief 获取插件目录
     */
    virtual std::string getPluginDirectory() const = 0;

    /**
     * @brief 获取数据目录
     */
    virtual std::string getDataDirectory() const = 0;

    /**
     * @brief 获取配置目录
     */
    virtual std::string getConfigDirectory() const = 0;

    /**
     * @brief 获取日志目录
     */
    virtual std::string getLogDirectory() const = 0;

    // ==================== 配置管理 ====================

    /**
     * @brief 获取配置值
     * @param key 配置键
     * @return 配置值（JSON字符串）
     */
    virtual std::string getConfig(const std::string& key) const = 0;

    /**
     * @brief 设置配置值
     * @param key 配置键
     * @param value 配置值（JSON字符串）
     */
    virtual void setConfig(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 获取所有配置
     * @return 配置（JSON字符串）
     */
    virtual std::string getAllConfig() const = 0;

    /**
     * @brief 保存配置到文件
     */
    virtual bool saveConfig() = 0;

    /**
     * @brief 重新加载配置
     */
    virtual bool reloadConfig() = 0;

    // ==================== 日志系统 ====================

    /**
     * @brief 获取日志记录器
     * @note 返回工作流系统的 Logger 实例
     */
    virtual WorkflowSystem::Logger& getLogger() = 0;

    /**
     * @brief 记录调试日志
     */
    virtual void logDebug(const std::string& message) = 0;

    /**
     * @brief 记录信息日志
     */
    virtual void logInfo(const std::string& message) = 0;

    /**
     * @brief 记录警告日志
     */
    virtual void logWarning(const std::string& message) = 0;

    /**
     * @brief 记录错误日志
     */
    virtual void logError(const std::string& message) = 0;

    // ==================== 事件系统 ====================

    /**
     * @brief 订阅事件
     * @param event 事件名称
     * @param handler 事件处理器
     * @return 订阅ID
     */
    virtual SubscriptionId subscribe(const std::string& event,
                                    EventHandler handler) = 0;

    /**
     * @brief 订阅事件（带优先级）
     */
    virtual SubscriptionId subscribe(const std::string& event,
                                    EventHandler handler,
                                    EventPriority priority) = 0;

    /**
     * @brief 取消订阅
     * @param subscriptionId 订阅ID
     */
    virtual void unsubscribe(SubscriptionId subscriptionId) = 0;

    /**
     * @brief 取消所有订阅
     */
    virtual void unsubscribeAll() = 0;

    /**
     * @brief 发布事件
     * @param event 事件对象
     * @return 事件结果
     */
    virtual EventResult publish(const Event& event) = 0;

    /**
     * @brief 异步发布事件
     */
    virtual void publishAsync(const Event& event) = 0;

    /**
     * @brief 发布事件（便捷方法）
     */
    virtual EventResult publish(const std::string& eventName,
                              const EventData& data = {}) = 0;

    /**
     * @brief 发布事件（便捷方法）
     */
    virtual void publishAsync(const std::string& eventName,
                             const EventData& data = {}) = 0;

    // ==================== 消息系统 ====================

    /**
     * @brief 发送消息（请求-响应）
     * @param to 目标插件ID
     * @param message 消息对象
     * @param timeout 超时时间
     * @return 响应
     */
    virtual Response sendMessage(const std::string& to,
                                const Message& message,
                                Duration timeout = Duration::fromSeconds(5)) = 0;

    /**
     * @brief 发送通知（无需响应）
     * @param to 目标插件ID
     * @param message 消息对象
     */
    virtual void sendNotification(const std::string& to,
                                  const Message& message) = 0;

    /**
     * @brief 广播消息
     * @param message 消息对象
     */
    virtual void broadcast(const Message& message) = 0;

    /**
     * @brief RPC调用
     * @param pluginId 目标插件ID
     * @param method 方法名
     * @param params 参数（JSON字符串）
     * @param timeout 超时时间
     * @return 结果（JSON字符串）
     */
    virtual std::string callRpc(const std::string& pluginId,
                              const std::string& method,
                              const std::string& params,
                              Duration timeout = Duration::fromSeconds(5)) = 0;

    // ==================== 插件发现 ====================

    /**
     * @brief 获取插件管理器
     */
    virtual IPluginManager* getPluginManager() = 0;

    /**
     * @brief 获取插件
     * @param pluginId 插件ID
     * @return 插件指针（如果不存在返回nullptr）
     */
    virtual IPlugin* getPlugin(const std::string& pluginId) const = 0;

    /**
     * @brief 获取所有插件
     */
    virtual std::vector<IPlugin*> getAllPlugins() const = 0;

    /**
     * @brief 检查插件是否存在
     */
    virtual bool hasPlugin(const std::string& pluginId) const = 0;

    /**
     * @brief 检查插件是否已加载
     */
    virtual bool isPluginLoaded(const std::string& pluginId) const = 0;

    // ==================== 数据通道 ====================

    /**
     * @brief 创建数据通道
     * @param channelId 通道ID
     * @return 数据通道指针
     */
    virtual IDataChannel* createDataChannel(const std::string& channelId) = 0;

    /**
     * @brief 获取数据通道
     * @param channelId 通道ID
     * @return 数据通道指针（如果不存在返回nullptr）
     */
    virtual IDataChannel* getDataChannel(const std::string& channelId) = 0;

    /**
     * @brief 关闭数据通道
     */
    virtual void closeDataChannel(const std::string& channelId) = 0;

    // ==================== 资源管理 ====================

    /**
     * @brief 注册共享资源
     * @param resourceId 资源ID
     * @param resource 资源指针
     * @param deleter 删除函数
     */
    virtual void registerResource(const std::string& resourceId,
                                 void* resource,
                                 std::function<void(void*)> deleter) = 0;

    /**
     * @brief 获取共享资源
     * @param resourceId 资源ID
     * @return 资源指针（如果不存在返回nullptr）
     */
    virtual void* getResource(const std::string& resourceId) const = 0;

    /**
     * @brief 注销共享资源
     */
    virtual void unregisterResource(const std::string& resourceId) = 0;

    // ==================== 定时器 ====================

    /**
     * @brief 延迟执行
     * @param delay 延迟时间
     * @param callback 回调函数
     * @return 定时器ID
     */
    virtual TimerId scheduleDelayed(Duration delay, Callback callback) = 0;

    /**
     * @brief 周期执行
     * @param interval 执行间隔
     * @param callback 回调函数
     * @return 定时器ID
     */
    virtual TimerId scheduleInterval(Duration interval, Callback callback) = 0;

    /**
     * @brief 取消定时器
     * @param timerId 定时器ID
     */
    virtual void cancelTimer(TimerId timerId) = 0;

    // ==================== 线程池 ====================

    /**
     * @brief 异步执行任务
     * @param task 任务函数
     */
    virtual void executeAsync(Callback task) = 0;

    /**
     * @brief 异步执行任务（带完成回调）
     * @param task 任务函数
     * @param onComplete 完成回调
     */
    virtual void executeAsync(Callback task, Callback onComplete) = 0;

    /**
     * @brief 获取线程池
     */
    virtual ThreadPool* getThreadPool() = 0;

    // ==================== 生命周期控制 ====================

    /**
     * @brief 请求重启插件
     */
    virtual bool requestRestart() = 0;

    /**
     * @brief 请求重新加载配置
     */
    virtual bool requestReloadConfig() = 0;

    /**
     * @brief 请求禁用插件
     */
    virtual bool requestDisable() = 0;

    // ==================== 通讯通道 ====================

    /**
     * @brief 获取或创建通道
     * @param name 通道名称
     * @return 通道的shared_ptr
     */
    virtual std::shared_ptr<Channel> getChannel(const std::string& name) = 0;

    /**
     * @brief 获取默认通道
     */
    virtual std::shared_ptr<Channel> getDefaultChannel() = 0;

    /**
     * @brief 订阅数据（便捷方法）
     * @param topic 主题
     * @param callback 回调函数
     * @return 订阅ID
     */
    virtual SubscriptionId subscribeData(const std::string& topic, 
                                          DataCallback callback) = 0;

    /**
     * @brief 取消数据订阅
     */
    virtual void unsubscribeData(SubscriptionId id) = 0;

    /**
     * @brief 发布数据（便捷方法，异步）
     */
    virtual void publishData(const std::string& topic, 
                            const std::map<std::string, std::string>& data) = 0;

    /**
     * @brief 发布数据（同步，直接调用回调）
     */
    virtual void publishDataSync(const std::string& topic,
                                 const std::map<std::string, std::string>& data) = 0;

    // ==================== 系统服务 ====================

    /**
     * @brief 获取系统服务
     * @param serviceName 服务名称
     * @return 服务指针（如果不存在返回nullptr）
     */
    virtual void* getService(const std::string& serviceName) const = 0;

    /**
     * @brief 注册系统服务
     * @param serviceName 服务名称
     * @param service 服务指针
     */
    virtual void registerService(const std::string& serviceName, void* service) = 0;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_IPLUGIN_CONTEXT_HPP
