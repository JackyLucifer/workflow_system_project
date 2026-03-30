#ifndef PLUGIN_FRAMEWORK_ASYNC_CHANNEL_HPP
#define PLUGIN_FRAMEWORK_ASYNC_CHANNEL_HPP

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 异步数据包
 */
struct AsyncDataPacket {
    std::string sourcePlugin;
    std::string targetPlugin;
    std::string channel;
    std::string topic;
    std::map<std::string, std::string> data;
    std::chrono::system_clock::time_point timestamp;
    uint64_t sequenceId;
    int priority;

    AsyncDataPacket() 
        : timestamp(std::chrono::system_clock::now())
        , sequenceId(0)
        , priority(0) {}

    std::string get(const std::string& key, const std::string& defaultValue = "") const {
        auto it = data.find(key);
        return (it != data.end()) ? it->second : defaultValue;
    }

    void set(const std::string& key, const std::string& value) {
        data[key] = value;
    }

    double getDouble(const std::string& key, double defaultValue = 0.0) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::stod(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    void setDouble(const std::string& key, double value) {
        data[key] = std::to_string(value);
    }

    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    void setInt(const std::string& key, int value) {
        data[key] = std::to_string(value);
    }
};

/**
 * @brief 异步数据处理器
 */
using AsyncDataHandler = std::function<void(const AsyncDataPacket&)>;

/**
 * @brief 异步通道订阅信息
 */
struct AsyncSubscription {
    uint64_t id;
    std::string pluginId;
    std::string channel;
    std::string topic;
    AsyncDataHandler handler;
    int priority;
    bool active;

    AsyncSubscription() : id(0), priority(0), active(true) {}
};

/**
 * @brief 订阅ID类型
 */
using AsyncSubscriptionId = uint64_t;

/**
 * @brief 异步通道 - 支持发布/订阅模式的数据通讯
 */
class AsyncChannel {
public:
    AsyncChannel() : running_(false), nextSubscriptionId_(1), nextSequenceId_(1) {}

    ~AsyncChannel() {
        stop();
    }

    /**
     * @brief 启动异步通道
     */
    void start() {
        if (running_.load()) {
            return;
        }

        running_.store(true);
        workerThread_ = std::thread(&AsyncChannel::processQueue, this);
    }

    /**
     * @brief 停止异步通道
     */
    void stop() {
        if (!running_.load()) {
            return;
        }

        running_.store(false);
        queueCondition_.notify_all();

        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

    /**
     * @brief 订阅通道
     * @param pluginId 插件ID
     * @param channel 通道名称
     * @param topic 主题（可选，空字符串表示订阅所有主题）
     * @param handler 数据处理器
     * @param priority 优先级（数值越大优先级越高）
     * @return 订阅ID
     */
    AsyncSubscriptionId subscribe(const std::string& pluginId,
                                   const std::string& channel,
                                   const std::string& topic,
                                   AsyncDataHandler handler,
                                   int priority = 0) {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);

        AsyncSubscription subscription;
        subscription.id = nextSubscriptionId_++;
        subscription.pluginId = pluginId;
        subscription.channel = channel;
        subscription.topic = topic;
        subscription.handler = handler;
        subscription.priority = priority;
        subscription.active = true;

        subscriptions_.push_back(subscription);

        // 按优先级排序
        std::sort(subscriptions_.begin(), subscriptions_.end(),
                  [](const AsyncSubscription& a, const AsyncSubscription& b) {
                      return a.priority > b.priority;
                  });

        return subscription.id;
    }

    /**
     * @brief 取消订阅
     */
    void unsubscribe(AsyncSubscriptionId subscriptionId) {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);

        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
            if (it->id == subscriptionId) {
                subscriptions_.erase(it);
                break;
            }
        }
    }

    /**
     * @brief 发布数据（异步）
     * @param packet 数据包
     */
    void publish(const AsyncDataPacket& packet) {
        if (!running_.load()) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            AsyncDataPacket p = packet;
            p.sequenceId = nextSequenceId_++;
            p.timestamp = std::chrono::system_clock::now();
            queue_.push(p);
        }

        queueCondition_.notify_one();
    }

    /**
     * @brief 发布数据（便捷方法）
     */
    void publish(const std::string& sourcePlugin,
                 const std::string& channel,
                 const std::string& topic,
                 const std::map<std::string, std::string>& data) {
        AsyncDataPacket packet;
        packet.sourcePlugin = sourcePlugin;
        packet.channel = channel;
        packet.topic = topic;
        packet.data = data;
        publish(packet);
    }

    /**
     * @brief 发送数据到指定插件（点对点）
     */
    void sendTo(const std::string& sourcePlugin,
                const std::string& targetPlugin,
                const std::string& channel,
                const std::map<std::string, std::string>& data) {
        AsyncDataPacket packet;
        packet.sourcePlugin = sourcePlugin;
        packet.targetPlugin = targetPlugin;
        packet.channel = channel;
        packet.data = data;
        publish(packet);
    }

    /**
     * @brief 同步发送并等待响应
     */
    std::future<AsyncDataPacket> sendAndWait(const std::string& sourcePlugin,
                                              const std::string& targetPlugin,
                                              const std::string& channel,
                                              const std::map<std::string, std::string>& data,
                                              std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        auto promise = std::make_shared<std::promise<AsyncDataPacket>>();
        auto future = promise->get_future();

        AsyncDataPacket packet;
        packet.sourcePlugin = sourcePlugin;
        packet.targetPlugin = targetPlugin;
        packet.channel = channel;
        packet.data = data;

        // 注册一次性响应处理器
        std::string responseChannel = channel + ".response." + std::to_string(packet.sequenceId);
        AsyncSubscriptionId subId = subscribe(sourcePlugin, responseChannel, "",
            [promise](const AsyncDataPacket& response) {
                promise->set_value(response);
            }, 100);

        publish(packet);

        // 设置超时检查
        std::thread timeoutThread([this, subId, promise, timeout]() {
            std::this_thread::sleep_for(timeout);
            try {
                promise->set_value(AsyncDataPacket());
            } catch (...) {
                // 已经设置过了
            }
            this->unsubscribe(subId);
        });
        timeoutThread.detach();

        return future;
    }

    /**
     * @brief 获取活跃订阅数量
     */
    size_t getActiveSubscriptionCount() const {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        size_t count = 0;
        for (const auto& sub : subscriptions_) {
            if (sub.active) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief 获取队列大小
     */
    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return queue_.size();
    }

    /**
     * @brief 获取指定通道的订阅者列表
     */
    std::vector<std::string> getSubscribers(const std::string& channel) const {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        std::vector<std::string> subscribers;
        for (const auto& sub : subscriptions_) {
            if (sub.channel == channel && sub.active) {
                subscribers.push_back(sub.pluginId);
            }
        }
        return subscribers;
    }

private:
    void processQueue() {
        while (running_.load()) {
            AsyncDataPacket packet;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCondition_.wait(lock, [this]() {
                    return !queue_.empty() || !running_.load();
                });

                if (!running_.load()) {
                    break;
                }

                if (queue_.empty()) {
                    continue;
                }

                packet = queue_.front();
                queue_.pop();
            }

            // 分发数据到订阅者
            dispatchToSubscribers(packet);
        }
    }

    void dispatchToSubscribers(const AsyncDataPacket& packet) {
        std::vector<AsyncSubscription> subsCopy;

        {
            std::lock_guard<std::mutex> lock(subscriptionsMutex_);
            subsCopy = subscriptions_;
        }

        for (const auto& sub : subsCopy) {
            if (!sub.active) {
                continue;
            }

            // 检查通道匹配
            if (sub.channel != packet.channel && sub.channel != "*") {
                continue;
            }

            // 检查目标插件（点对点消息）
            if (!packet.targetPlugin.empty() && sub.pluginId != packet.targetPlugin) {
                continue;
            }

            // 检查主题匹配
            if (!sub.topic.empty() && sub.topic != packet.topic && sub.topic != "*") {
                continue;
            }

            // 异步调用处理器
            try {
                sub.handler(packet);
            } catch (...) {
                // 忽略处理器异常
            }
        }
    }

    std::atomic<bool> running_;
    std::thread workerThread_;

    std::vector<AsyncSubscription> subscriptions_;
    mutable std::mutex subscriptionsMutex_;

    std::queue<AsyncDataPacket> queue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    std::atomic<uint64_t> nextSubscriptionId_;
    std::atomic<uint64_t> nextSequenceId_;
};

/**
 * @brief 全局异步通道管理器
 */
class AsyncChannelManager {
public:
    static AsyncChannelManager& getInstance() {
        static AsyncChannelManager instance;
        return instance;
    }

    /**
     * @brief 获取或创建通道
     */
    std::shared_ptr<AsyncChannel> getChannel(const std::string& name) {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        
        auto it = channels_.find(name);
        if (it != channels_.end()) {
            return it->second;
        }

        auto channel = std::shared_ptr<AsyncChannel>(new AsyncChannel());
        channel->start();
        channels_[name] = channel;
        return channel;
    }

    /**
     * @brief 获取默认通道
     */
    std::shared_ptr<AsyncChannel> getDefaultChannel() {
        return getChannel("default");
    }

    /**
     * @brief 关闭指定通道
     */
    void closeChannel(const std::string& name) {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        auto it = channels_.find(name);
        if (it != channels_.end()) {
            it->second->stop();
            channels_.erase(it);
        }
    }

    /**
     * @brief 关闭所有通道
     */
    void closeAll() {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        for (auto& pair : channels_) {
            pair.second->stop();
        }
        channels_.clear();
    }

    /**
     * @brief 获取所有通道名称
     */
    std::vector<std::string> getChannelNames() const {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        std::vector<std::string> names;
        for (const auto& pair : channels_) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    AsyncChannelManager() = default;
    ~AsyncChannelManager() {
        closeAll();
    }

    AsyncChannelManager(const AsyncChannelManager&) = delete;
    AsyncChannelManager& operator=(const AsyncChannelManager&) = delete;

    std::map<std::string, std::shared_ptr<AsyncChannel>> channels_;
    mutable std::mutex channelsMutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_ASYNC_CHANNEL_HPP
