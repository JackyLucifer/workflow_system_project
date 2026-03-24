/**
 * @file message_bus.h
 * @brief 消息总线 - 插件间通信
 */

#ifndef WORKFLOW_SYSTEM_MESSAGE_BUS_H
#define WORKFLOW_SYSTEM_MESSAGE_BUS_H

#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <map>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include <algorithm>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 消息类型
 */
using MessageType = uint32_t;

/**
 * @brief 消息优先级
 */
enum class MessagePriority : int {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    URGENT = 3
};

/**
 * @brief 消息
 */
struct Message {
    MessageType type;
    std::string topic;
    std::string source;
    std::string target;
    std::string data;
    int64_t timestamp;
    MessagePriority priority;
    uint64_t messageId;
    bool broadcast;
    int ttl;
    
    Message()
        : type(0)
        , timestamp(0)
        , priority(MessagePriority::NORMAL)
        , messageId(0)
        , broadcast(true)
        , ttl(10) {}
    
    static Message create(const std::string& topic, const std::string& data = "") {
        Message msg;
        msg.topic = topic;
        msg.data = data;
        msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
        return msg;
    }
    
    Message& from(const std::string& src) {
        source = src;
        return *this;
    }
    
    Message& to(const std::string& tgt) {
        target = tgt;
        broadcast = false;
        return *this;
    }
    
    Message& withPriority(MessagePriority prio) {
        priority = prio;
        return *this;
    }
    
    Message& withType(MessageType t) {
        type = t;
        return *this;
    }
};

/**
 * @brief 消息过滤器
 */
using MessageFilter = std::function<bool(const Message&)>;

/**
 * @brief 消息处理器
 */
using MessageHandler = std::function<void(const Message&)>;

/**
 * @brief 订阅信息
 */
struct Subscription {
    uint64_t subscriptionId;
    std::string topic;
    std::string subscriber;
    MessageFilter filter;
    MessageHandler handler;
    bool active;
    int priority;
    
    Subscription() : subscriptionId(0), active(true), priority(0) {}
};

/**
 * @brief 消息统计
 */
struct MessageStats {
    uint64_t totalSent;
    uint64_t totalDelivered;
    uint64_t totalDropped;
    uint64_t totalSubscribers;
    std::map<std::string, uint64_t> topicCounts;
    
    MessageStats() : totalSent(0), totalDelivered(0), totalDropped(0), totalSubscribers(0) {}
};

/**
 * @brief 消息总线
 */
class MessageBus {
public:
    static MessageBus& getInstance() {
        static MessageBus instance;
        return instance;
    }
    
    // ========== 订阅 ==========
    
    /**
     * @brief 订阅主题
     */
    uint64_t subscribe(const std::string& topic, 
                      MessageHandler handler,
                      const std::string& subscriber = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint64_t subId = nextSubscriptionId_++;
        
        Subscription sub;
        sub.subscriptionId = subId;
        sub.topic = topic;
        sub.subscriber = subscriber;
        sub.handler = handler;
        sub.active = true;
        sub.priority = 0;
        
        subscriptions_[subId] = sub;
        topicSubscriptions_[topic].push_back(subId);
        
        return subId;
    }
    
    /**
     * @brief 带过滤器的订阅
     */
    uint64_t subscribeWithFilter(const std::string& topic,
                                MessageHandler handler,
                                MessageFilter filter,
                                const std::string& subscriber = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint64_t subId = nextSubscriptionId_++;
        
        Subscription sub;
        sub.subscriptionId = subId;
        sub.topic = topic;
        sub.subscriber = subscriber;
        sub.handler = handler;
        sub.filter = filter;
        sub.active = true;
        sub.priority = 0;
        
        subscriptions_[subId] = sub;
        topicSubscriptions_[topic].push_back(subId);
        
        return subId;
    }
    
    /**
     * @brief 取消订阅
     */
    bool unsubscribe(uint64_t subscriptionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = subscriptions_.find(subscriptionId);
        if (it == subscriptions_.end()) {
            return false;
        }
        
        std::string topic = it->second.topic;
        auto& subList = topicSubscriptions_[topic];
        subList.erase(std::remove(subList.begin(), subList.end(), subscriptionId), subList.end());
        
        subscriptions_.erase(it);
        return true;
    }
    
    /**
     * @brief 取消所有订阅
     */
    void unsubscribeAll(const std::string& subscriber) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint64_t> toRemove;
        for (const auto& pair : subscriptions_) {
            if (pair.second.subscriber == subscriber) {
                toRemove.push_back(pair.first);
            }
        }
        
        for (uint64_t id : toRemove) {
            unsubscribeInternal(id);
        }
    }
    
    // ========== 发布 ==========
    
    /**
     * @brief 发布消息
     */
    void publish(const Message& message) {
        std::vector<Subscription> handlersToCall;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.totalSent++;
            
            Message msg = message;
            msg.messageId = nextMessageId_++;
            
            auto it = topicSubscriptions_.find(msg.topic);
            if (it == topicSubscriptions_.end()) {
                stats_.totalDropped++;
                return;
            }
            
            for (uint64_t subId : it->second) {
                auto subIt = subscriptions_.find(subId);
                if (subIt != subscriptions_.end() && subIt->second.active) {
                    if (!subIt->second.filter || subIt->second.filter(msg)) {
                        handlersToCall.push_back(subIt->second);
                    }
                }
            }
            
            stats_.topicCounts[msg.topic]++;
        }
        
        for (const auto& sub : handlersToCall) {
            try {
                sub.handler(message);
                stats_.totalDelivered++;
            } catch (...) {
                stats_.totalDropped++;
            }
        }
    }
    
    /**
     * @brief 发布消息（简化版）
     */
    void publish(const std::string& topic, const std::string& data = "") {
        Message msg = Message::create(topic, data);
        publish(msg);
    }
    
    /**
     * @brief 同步发布（等待所有处理器完成）
     */
    void publishSync(const Message& message) {
        publish(message);
    }
    
    /**
     * @brief 发送点对点消息
     */
    bool send(const std::string& target, const Message& message) {
        Message msg = message;
        msg.target = target;
        msg.broadcast = false;
        publish(msg);
        return true;
    }
    
    // ========== 查询 ==========
    
    /**
     * @brief 获取主题的订阅者数量
     */
    size_t getSubscriberCount(const std::string& topic) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topicSubscriptions_.find(topic);
        return (it != topicSubscriptions_.end()) ? it->second.size() : 0;
    }
    
    /**
     * @brief 获取所有主题
     */
    std::vector<std::string> getTopics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> topics;
        for (const auto& pair : topicSubscriptions_) {
            topics.push_back(pair.first);
        }
        return topics;
    }
    
    /**
     * @brief 获取统计信息
     */
    MessageStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    /**
     * @brief 重置统计
     */
    void resetStats() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = MessageStats();
    }
    
    // ========== 管理 ==========
    
    /**
     * @brief 清空所有订阅
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        subscriptions_.clear();
        topicSubscriptions_.clear();
    }
    
    /**
     * @brief 暂停订阅
     */
    bool pauseSubscription(uint64_t subscriptionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(subscriptionId);
        if (it != subscriptions_.end()) {
            it->second.active = false;
            return true;
        }
        return false;
    }
    
    /**
     * @brief 恢复订阅
     */
    bool resumeSubscription(uint64_t subscriptionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(subscriptionId);
        if (it != subscriptions_.end()) {
            it->second.active = true;
            return true;
        }
        return false;
    }

private:
    MessageBus() 
        : nextSubscriptionId_(1)
        , nextMessageId_(1) {}
    
    mutable std::mutex mutex_;
    
    std::unordered_map<uint64_t, Subscription> subscriptions_;
    std::map<std::string, std::vector<uint64_t>> topicSubscriptions_;
    
    uint64_t nextSubscriptionId_;
    uint64_t nextMessageId_;
    
    MessageStats stats_;
    
    void unsubscribeInternal(uint64_t subscriptionId) {
        auto it = subscriptions_.find(subscriptionId);
        if (it == subscriptions_.end()) return;
        
        std::string topic = it->second.topic;
        auto& subList = topicSubscriptions_[topic];
        subList.erase(std::remove(subList.begin(), subList.end(), subscriptionId), subList.end());
        
        subscriptions_.erase(it);
    }
};

// 预定义消息类型
namespace MessageTypes {
    constexpr MessageType SYSTEM = 0;
    constexpr MessageType PLUGIN = 1;
    constexpr MessageType DATA = 2;
    constexpr MessageType COMMAND = 3;
    constexpr MessageType EVENT = 4;
    constexpr MessageType ERROR = 5;
    constexpr MessageType LOG = 6;
}

// 预定义主题
namespace MessageTopics {
    const std::string PLUGIN_LOADED = "plugin.loaded";
    const std::string PLUGIN_UNLOADED = "plugin.unloaded";
    const std::string PLUGIN_STARTED = "plugin.started";
    const std::string PLUGIN_STOPPED = "plugin.stopped";
    const std::string PLUGIN_ERROR = "plugin.error";
    const std::string SERVICE_REGISTERED = "service.registered";
    const std::string SERVICE_UNREGISTERED = "service.unregistered";
    const std::string CONFIG_CHANGED = "config.changed";
    const std::string SHUTDOWN_REQUEST = "system.shutdown";
}

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_MESSAGE_BUS_H
