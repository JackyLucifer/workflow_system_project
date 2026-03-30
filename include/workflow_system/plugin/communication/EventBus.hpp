#ifndef PLUGIN_FRAMEWORK_EVENT_BUS_HPP
#define PLUGIN_FRAMEWORK_EVENT_BUS_HPP

#include "Event.hpp"
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 事件总线接口
 */
class IEventBus {
public:
    virtual ~IEventBus() = default;

    /**
     * @brief 订阅事件
     */
    virtual SubscriptionId subscribe(const std::string& subscriberId,
                                     const std::string& eventName,
                                     EventHandler handler,
                                     EventPriority priority = EventPriority::NORMAL) = 0;

    /**
     * @brief 订阅一次性事件
     */
    virtual SubscriptionId subscribeOnce(const std::string& subscriberId,
                                        const std::string& eventName,
                                        EventHandler handler,
                                        EventPriority priority = EventPriority::NORMAL) = 0;

    /**
     * @brief 取消订阅
     */
    virtual bool unsubscribe(SubscriptionId subscriptionId) = 0;

    /**
     * @brief 取消订阅者的所有订阅
     */
    virtual size_t unsubscribeAll(const std::string& subscriberId) = 0;

    /**
     * @brief 发布事件（同步）
     */
    virtual EventResult publish(const IEvent& event) = 0;

    /**
     * @brief 发布事件（异步）
     */
    virtual void publishAsync(const IEvent& event) = 0;

    /**
     * @brief 获取事件的订阅者数量
     */
    virtual size_t getSubscriberCount(const std::string& eventName) const = 0;

    /**
     * @brief 获取所有订阅
     */
    virtual std::vector<Subscription> getAllSubscriptions() const = 0;

    /**
     * @brief 添加事件过滤器
     */
    virtual void addEventFilter(EventFilter filter) = 0;

    /**
     * @brief 清除事件过滤器
     */
    virtual void clearEventFilters() = 0;

    /**
     * @brief 启动异步处理
     */
    virtual void startAsyncProcessing() = 0;

    /**
     * @brief 停止异步处理
     */
    virtual void stopAsyncProcessing() = 0;

    /**
     * @brief 等待所有异步事件处理完成
     */
    virtual void waitForAsyncEvents() = 0;
};

/**
 * @brief 事件总线实现
 */
class EventBus : public IEventBus {
public:
    EventBus();
    explicit EventBus(size_t threadCount);
    ~EventBus() override;

    // 订阅管理
    SubscriptionId subscribe(const std::string& subscriberId,
                            const std::string& eventName,
                            EventHandler handler,
                            EventPriority priority = EventPriority::NORMAL) override;

    SubscriptionId subscribeOnce(const std::string& subscriberId,
                                const std::string& eventName,
                                EventHandler handler,
                                EventPriority priority = EventPriority::NORMAL) override;

    bool unsubscribe(SubscriptionId subscriptionId) override;
    size_t unsubscribeAll(const std::string& subscriberId) override;

    // 事件发布
    EventResult publish(const IEvent& event) override;
    void publishAsync(const IEvent& event) override;

    // 查询
    size_t getSubscriberCount(const std::string& eventName) const override;
    std::vector<Subscription> getAllSubscriptions() const override;

    // 过滤器
    void addEventFilter(EventFilter filter) override;
    void clearEventFilters() override;

    // 异步处理
    void startAsyncProcessing() override;
    void stopAsyncProcessing() override;
    void waitForAsyncEvents() override;

    // 统计
    struct Statistics {
        size_t totalSubscriptions = 0;
        size_t totalEventsPublished = 0;
        size_t totalEventsProcessed = 0;
        size_t asyncQueueSize = 0;
        size_t failedEvents = 0;
    };

    Statistics getStatistics() const;
    void resetStatistics();

private:
    // 内部处理方法
    bool applyFilters(const IEvent& event);
    void processAsyncQueue();
    void notifySubscribers(const IEvent& event, std::vector<std::shared_ptr<Subscription>>& subscriptions);

    // 订阅存储（按事件名分组）
    using SubscriptionMap = std::map<std::string, std::vector<std::shared_ptr<Subscription>>>;
    using SubscriptionIndex = std::map<SubscriptionId, std::shared_ptr<Subscription>>;

    SubscriptionMap subscriptions_;
    SubscriptionIndex subscriptionIndex_;
    mutable std::mutex subscriptionsMutex_;

    // 订阅ID生成器
    std::atomic<SubscriptionId> nextSubscriptionId_{1};

    // 事件过滤器
    std::vector<EventFilter> filters_;
    std::mutex filtersMutex_;

    // 异步处理
    std::queue<std::shared_ptr<Event>> asyncQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> asyncRunning_{false};
    size_t threadCount_;

    // 统计
    mutable std::mutex statsMutex_;
    Statistics statistics_;
};

/**
 * @brief 事件作用域（自动取消订阅）
 */
class EventScope {
public:
    EventScope(IEventBus& bus, const std::string& subscriberId)
        : bus_(bus), subscriberId_(subscriberId) {}

    ~EventScope() {
        bus_.unsubscribeAll(subscriberId_);
    }

    // 禁止拷贝
    EventScope(const EventScope&) = delete;
    EventScope& operator=(const EventScope&) = delete;

private:
    IEventBus& bus_;
    std::string subscriberId_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_EVENT_BUS_HPP
