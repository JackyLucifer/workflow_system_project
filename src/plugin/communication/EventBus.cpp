#include "workflow_system/plugin/communication/EventBus.hpp"
#include "workflow_system/core/logger.h"
#include <algorithm>
#include <stdexcept>

namespace WorkflowSystem { namespace Plugin {

EventBus::EventBus()
    : EventBus(std::thread::hardware_concurrency() > 0 ?
               std::thread::hardware_concurrency() : 4) {}

EventBus::EventBus(size_t threadCount)
    : threadCount_(threadCount) {
}

EventBus::~EventBus() {
    stopAsyncProcessing();
}

SubscriptionId EventBus::subscribe(const std::string& subscriberId,
                                  const std::string& eventName,
                                  EventHandler handler,
                                  EventPriority priority) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    auto subscription = std::make_shared<Subscription>(
        nextSubscriptionId_++,
        subscriberId,
        eventName,
        handler,
        priority
    );

    subscriptions_[eventName].push_back(subscription);
    subscriptionIndex_[subscription->id] = subscription;

    // 按优先级排序（高优先级在前）
    std::sort(subscriptions_[eventName].begin(),
              subscriptions_[eventName].end(),
              [](const std::shared_ptr<Subscription>& a, 
                 const std::shared_ptr<Subscription>& b) {
                  return static_cast<int>(a->priority) > static_cast<int>(b->priority);
              });

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalSubscriptions++;
    }

    LOG_INFO("订阅事件: " + eventName + " 订阅者: " + subscriberId);

    return subscription->id;
}

SubscriptionId EventBus::subscribeOnce(const std::string& subscriberId,
                                      const std::string& eventName,
                                      EventHandler handler,
                                      EventPriority priority) {
    auto id = subscribe(subscriberId, eventName, handler, priority);

    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    auto it = subscriptionIndex_.find(id);
    if (it != subscriptionIndex_.end()) {
        it->second->once = true;
    }

    return id;
}

bool EventBus::unsubscribe(SubscriptionId subscriptionId) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    auto indexIt = subscriptionIndex_.find(subscriptionId);
    if (indexIt == subscriptionIndex_.end()) {
        return false;
    }

    auto subscription = indexIt->second;
    auto eventIt = subscriptions_.find(subscription->eventName);

    if (eventIt != subscriptions_.end()) {
        auto& subs = eventIt->second;
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                           [&subscriptionId](const std::shared_ptr<Subscription>& sub) {
                               return sub->id == subscriptionId;
                           }),
            subs.end()
        );

        if (subs.empty()) {
            subscriptions_.erase(eventIt);
        }
    }

    subscriptionIndex_.erase(indexIt);

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalSubscriptions--;
    }

    return true;
}

size_t EventBus::unsubscribeAll(const std::string& subscriberId) {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    size_t count = 0;

    for (auto& eventPair : subscriptions_) {
        auto& subs = eventPair.second;
        auto originalSize = subs.size();

        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                           [&subscriberId](const std::shared_ptr<Subscription>& sub) {
                               return sub->subscriberId == subscriberId;
                           }),
            subs.end()
        );

        count += (originalSize - subs.size());

        // 同时从索引中移除
        for (auto it = subscriptionIndex_.begin(); it != subscriptionIndex_.end();) {
            if (it->second->subscriberId == subscriberId) {
                it = subscriptionIndex_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 清理空的事件列表
    for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
        if (it->second.empty()) {
            it = subscriptions_.erase(it);
        } else {
            ++it;
        }
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalSubscriptions -= count;
    }

    return count;
}

EventResult EventBus::publish(const IEvent& event) {
    // 应用过滤器
    if (!applyFilters(event)) {
        return EventResult{};
    }

    std::vector<std::shared_ptr<Subscription>> subscribers;
    {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);

        auto it = subscriptions_.find(event.getName());
        if (it == subscriptions_.end() || it->second.empty()) {
            return EventResult{};
        }

        // 复制订阅者列表（避免在通知过程中持有锁）
        subscribers = it->second;
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalEventsPublished++;
    }

    EventResult result;
    result.subscriberCount = subscribers.size();

    notifySubscribers(event, subscribers);

    // 统计成功和失败
    for (const auto& sub : subscribers) {
        if (sub->active) {
            result.successCount++;
        } else {
            result.failureCount++;
        }
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalEventsProcessed += result.successCount;
    }

    return result;
}

void EventBus::publishAsync(const IEvent& event) {
    if (!applyFilters(event)) {
        return;
    }

    // 如果异步处理未启动，则同步处理
    if (!asyncRunning_.load()) {
        publish(event);
        return;
    }

    auto eventCopy = std::make_shared<Event>(
        event.getName(),
        event.getSource(),
        event.getData(),
        event.getTarget(),
        event.getPriority()
    );

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        asyncQueue_.push(eventCopy);
    }

    queueCondition_.notify_one();
}

size_t EventBus::getSubscriberCount(const std::string& eventName) const {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    auto it = subscriptions_.find(eventName);
    return (it != subscriptions_.end()) ? it->second.size() : 0;
}

std::vector<Subscription> EventBus::getAllSubscriptions() const {
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    std::vector<Subscription> result;
    result.reserve(subscriptionIndex_.size());

    for (const auto& pair : subscriptionIndex_) {
        result.push_back(*pair.second);
    }

    return result;
}

void EventBus::addEventFilter(EventFilter filter) {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    filters_.push_back(filter);
}

void EventBus::clearEventFilters() {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    filters_.clear();
}

void EventBus::startAsyncProcessing() {
    if (asyncRunning_.load()) {
        return;  // 已经在运行
    }

    asyncRunning_.store(true);

    for (size_t i = 0; i < threadCount_; ++i) {
        workerThreads_.emplace_back([this]() {
            processAsyncQueue();
        });
    }

    LOG_INFO("事件总线异步处理已启动，线程数: " + std::to_string(threadCount_));
}

void EventBus::stopAsyncProcessing() {
    if (!asyncRunning_.load()) {
        return;
    }

    asyncRunning_.store(false);
    queueCondition_.notify_all();

    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    workerThreads_.clear();
    LOG_INFO("事件总线异步处理已停止");
}

void EventBus::waitForAsyncEvents() {
    std::unique_lock<std::mutex> lock(queueMutex_);

    queueCondition_.wait(lock, [this]() {
        return asyncQueue_.empty() || !asyncRunning_.load();
    });
}

EventBus::Statistics EventBus::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    Statistics stats = statistics_;
    stats.asyncQueueSize = asyncQueue_.size();

    return stats;
}

void EventBus::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_ = Statistics{};
}

bool EventBus::applyFilters(const IEvent& event) {
    std::lock_guard<std::mutex> lock(filtersMutex_);

    for (const auto& filter : filters_) {
        if (!filter(event)) {
            return false;  // 被过滤器拦截
        }
    }

    return true;
}

void EventBus::processAsyncQueue() {
    while (asyncRunning_.load()) {
        std::shared_ptr<Event> event;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait(lock, [this]() {
                return !asyncQueue_.empty() || !asyncRunning_.load();
            });

            if (!asyncRunning_.load()) {
                break;
            }

            if (asyncQueue_.empty()) {
                continue;
            }

            event = asyncQueue_.front();
            asyncQueue_.pop();
        }

        if (event) {
            publish(*event);
        }
    }
}

void EventBus::notifySubscribers(const IEvent& event,
                                std::vector<std::shared_ptr<Subscription>>& subscriptions) {
    std::vector<SubscriptionId> toRemove;

    for (auto& subscription : subscriptions) {
        if (!subscription->active) {
            continue;
        }

        try {
            subscription->handler(event);

            // 如果是一次性订阅，标记为移除
            if (subscription->once) {
                toRemove.push_back(subscription->id);
            }

            // 检查是否应该停止传播
            if (!event.shouldPropagate()) {
                break;
            }

        } catch (const std::exception& e) {
            LOG_ERROR("事件处理器异常: " + std::string(e.what()));
            subscription->active = false;

            {
                std::lock_guard<std::mutex> statsLock(statsMutex_);
                statistics_.failedEvents++;
            }
        }
    }

    // 移除一次性订阅
    for (auto id : toRemove) {
        unsubscribe(id);
    }
}

} // namespace Plugin
} // namespace WorkflowSystem
