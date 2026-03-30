#ifndef PLUGIN_FRAMEWORK_MESSAGE_BUS_HPP
#define PLUGIN_FRAMEWORK_MESSAGE_BUS_HPP

#include "Message.hpp"
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>
#include <future>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 消息处理器注册信息
 */
struct MessageHandlerRegistration {
    std::string handlerId;
    std::string pluginId;
    std::string method;
    MessageHandler handler;
    AsyncMessageHandler asyncHandler;
    bool active = true;

    MessageHandlerRegistration() = default;

    MessageHandlerRegistration(const std::string& hid, const std::string& pid,
                              const std::string& m, MessageHandler h)
        : handlerId(hid), pluginId(pid), method(m), handler(h) {}

    MessageHandlerRegistration(const std::string& hid, const std::string& pid,
                              const std::string& m, AsyncMessageHandler h)
        : handlerId(hid), pluginId(pid), method(m), asyncHandler(h) {}
};

/**
 * @brief 消息待处理项
 */
struct PendingMessage {
    Message message;
    std::shared_ptr<std::promise<Response>> responsePromise;
    std::chrono::system_clock::time_point deadline;

    bool isExpired() const {
        return std::chrono::system_clock::now() > deadline;
    }
};

/**
 * @brief 消息总线接口
 */
class IMessageBus {
public:
    virtual ~IMessageBus() = default;

    /**
     * @brief 注册消息处理器
     */
    virtual bool registerHandler(const std::string& pluginId,
                                const std::string& method,
                                MessageHandler handler) = 0;

    /**
     * @brief 注册异步消息处理器
     */
    virtual bool registerAsyncHandler(const std::string& pluginId,
                                     const std::string& method,
                                     AsyncMessageHandler handler) = 0;

    /**
     * @brief 注销处理器
     */
    virtual bool unregisterHandler(const std::string& pluginId,
                                  const std::string& method) = 0;

    /**
     * @brief 注销插件的所有处理器
     */
    virtual size_t unregisterAllHandlers(const std::string& pluginId) = 0;

    /**
     * @brief 发送消息（同步，等待响应）
     */
    virtual Response sendMessage(const Message& message,
                                uint64_t timeoutMs = 5000) = 0;

    /**
     * @brief 发送通知（无需响应）
     */
    virtual void sendNotification(const Message& message) = 0;

    /**
     * @brief 广播消息到所有插件
     */
    virtual void broadcast(const Message& message) = 0;

    /**
     * @brief 异步发送消息
     */
    virtual std::future<Response> sendMessageAsync(const Message& message,
                                                  uint64_t timeoutMs = 5000) = 0;

    /**
     * @brief 处理消息
     */
    virtual Response handleMessage(const Message& message) = 0;

    /**
     * @brief 获取处理器数量
     */
    virtual size_t getHandlerCount() const = 0;

    /**
     * @brief 获取插件的所有方法
     */
    virtual std::vector<std::string> getPluginMethods(const std::string& pluginId) const = 0;

    /**
     * @brief 启动后台处理
     */
    virtual void startBackgroundProcessing() = 0;

    /**
     * @brief 停止后台处理
     */
    virtual void stopBackgroundProcessing() = 0;

    /**
     * @brief 等待所有消息处理完成
     */
    virtual void waitForCompletion() = 0;
};

/**
 * @brief 消息总线实现
 */
class MessageBus : public IMessageBus {
public:
    MessageBus();
    explicit MessageBus(size_t threadCount);
    ~MessageBus() override;

    // 处理器注册
    bool registerHandler(const std::string& pluginId,
                        const std::string& method,
                        MessageHandler handler) override;

    bool registerAsyncHandler(const std::string& pluginId,
                             const std::string& method,
                             AsyncMessageHandler handler) override;

    bool unregisterHandler(const std::string& pluginId,
                          const std::string& method) override;

    size_t unregisterAllHandlers(const std::string& pluginId) override;

    // 消息发送
    Response sendMessage(const Message& message,
                        uint64_t timeoutMs = 5000) override;

    void sendNotification(const Message& message) override;

    void broadcast(const Message& message) override;

    std::future<Response> sendMessageAsync(const Message& message,
                                         uint64_t timeoutMs = 5000) override;

    Response handleMessage(const Message& message) override;

    // 查询
    size_t getHandlerCount() const override;
    std::vector<std::string> getPluginMethods(const std::string& pluginId) const override;

    // 后台处理
    void startBackgroundProcessing() override;
    void stopBackgroundProcessing() override;
    void waitForCompletion() override;

    // 统计
    struct Statistics {
        size_t totalHandlers = 0;
        size_t totalMessagesSent = 0;
        size_t totalNotificationsSent = 0;
        size_t totalBroadcasts = 0;
        size_t pendingMessages = 0;
        size_t timeoutMessages = 0;
        size_t failedMessages = 0;
    };

    Statistics getStatistics() const;
    void resetStatistics();

private:
    // 内部处理方法
    Response processMessage(const Message& message);
    void processNotificationQueue();
    std::string generateHandlerId();

    // 超时检测
    void checkTimeouts();

    // 处理器存储
    using HandlerKey = std::pair<std::string, std::string>;  // (pluginId, method)
    using HandlerMap = std::map<HandlerKey, std::shared_ptr<MessageHandlerRegistration>>;

    HandlerMap handlers_;
    mutable std::mutex handlersMutex_;

    // 待处理消息队列
    std::queue<PendingMessage> pendingMessages_;
    std::mutex pendingMutex_;
    std::condition_variable pendingCondition_;

    // 通知队列
    std::queue<Message> notificationQueue_;
    std::mutex notificationMutex_;
    std::condition_variable notificationCondition_;

    // 后台处理线程
    std::vector<std::thread> workerThreads_;
    std::thread notificationThread_;
    std::thread timeoutThread_;
    std::atomic<bool> running_{false};
    size_t threadCount_;

    // 统计
    mutable std::mutex statsMutex_;
    Statistics statistics_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_MESSAGE_BUS_HPP
