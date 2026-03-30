#ifndef PLUGIN_FRAMEWORK_CHANNEL_HPP
#define PLUGIN_FRAMEWORK_CHANNEL_HPP

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
#include <cstdint>

namespace WorkflowSystem { namespace Plugin {

// 数据包 - 简单的键值对
struct DataPacket {
    std::string topic;
    std::map<std::string, std::string> data;
    std::string source;
    
    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = data.find(key);
        return (it != data.end()) ? it->second : def;
    }
    
    double getDouble(const std::string& key, double def = 0.0) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try { return std::stod(it->second); } catch (...) {}
        }
        return def;
    }
    
    int getInt(const std::string& key, int def = 0) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try { return std::stoi(it->second); } catch (...) {}
        }
        return def;
    }
    
    void set(const std::string& key, const std::string& value) {
        data[key] = value;
    }
    
    void setDouble(const std::string& key, double value) {
        data[key] = std::to_string(value);
    }
    
    void setInt(const std::string& key, int value) {
        data[key] = std::to_string(value);
    }
};

// 数据回调类型
using DataCallback = std::function<void(const DataPacket&)>;

// 订阅ID
using SubscriptionId = uint64_t;

/**
 * @brief 通讯通道 - 支持异步发布/订阅
 * 
 * 插件通过获得此对象的指针或引用来使用通讯功能
 */
class Channel {
public:
    explicit Channel(const std::string& name) 
        : name_(name), running_(false), nextSubId_(1) {}
    
    ~Channel() {
        stop();
    }
    
    const std::string& name() const { return name_; }
    
    // 启动通道（启动异步处理线程）
    void start() {
        if (running_.load()) return;
        running_.store(true);
        worker_ = std::thread(&Channel::processLoop, this);
    }
    
    // 停止通道
    void stop() {
        if (!running_.load()) return;
        running_.store(false);
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    
    // 订阅主题，返回订阅ID
    SubscriptionId subscribe(const std::string& topic, DataCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        SubscriptionId id = nextSubId_++;
        subs_.push_back({id, topic, callback});
        return id;
    }
    
    // 取消订阅
    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = subs_.begin(); it != subs_.end(); ++it) {
            if (it->id == id) {
                subs_.erase(it);
                break;
            }
        }
    }
    
    // 异步发布（触发订阅者回调）
    void publish(const DataPacket& packet) {
        if (!running_.load()) return;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            queue_.push(packet);
        }
        cv_.notify_one();
    }
    
    // 便捷发布方法
    void publish(const std::string& topic, 
                 const std::string& source,
                 const std::map<std::string, std::string>& data) {
        DataPacket p;
        p.topic = topic;
        p.source = source;
        p.data = data;
        publish(p);
    }
    
    // 同步发布（直接调用，不走队列）
    void publishSync(const DataPacket& packet) {
        dispatch(packet);
    }
    
    // 获取订阅者数量
    size_t subscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return subs_.size();
    }
    
    // 获取队列大小
    size_t queueSize() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return queue_.size();
    }

private:
    struct Sub {
        SubscriptionId id;
        std::string topic;
        DataCallback callback;
    };
    
    void processLoop() {
        while (running_.load()) {
            DataPacket packet;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                cv_.wait(lock, [this] { return !queue_.empty() || !running_.load(); });
                if (!running_.load()) break;
                if (queue_.empty()) continue;
                packet = queue_.front();
                queue_.pop();
            }
            dispatch(packet);
        }
    }
    
    void dispatch(const DataPacket& packet) {
        std::vector<Sub> subsCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            subsCopy = subs_;
        }
        
        for (const auto& sub : subsCopy) {
            // 空主题表示订阅所有，否则需要匹配
            if (sub.topic.empty() || sub.topic == "*" || sub.topic == packet.topic) {
                try {
                    sub.callback(packet);
                } catch (...) {
                    // 忽略回调异常
                }
            }
        }
    }
    
    std::string name_;
    std::atomic<bool> running_;
    std::thread worker_;
    
    std::vector<Sub> subs_;
    mutable std::mutex mutex_;
    
    std::queue<DataPacket> queue_;
    mutable std::mutex queueMutex_;
    std::condition_variable cv_;
    
    std::atomic<SubscriptionId> nextSubId_;
};

/**
 * @brief 通道管理器 - 管理多个通讯通道
 */
class ChannelManager {
public:
    static ChannelManager& instance() {
        static ChannelManager mgr;
        return mgr;
    }
    
    // 获取或创建通道
    std::shared_ptr<Channel> get(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(name);
        if (it != channels_.end()) return it->second;
        
        auto ch = std::make_shared<Channel>(name);
        ch->start();
        channels_[name] = ch;
        return ch;
    }
    
    // 获取默认通道
    std::shared_ptr<Channel> getDefault() {
        return get("default");
    }
    
    // 删除通道
    void remove(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(name);
        if (it != channels_.end()) {
            it->second->stop();
            channels_.erase(it);
        }
    }
    
    // 获取所有通道名
    std::vector<std::string> names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& p : channels_) {
            result.push_back(p.first);
        }
        return result;
    }
    
    // 关闭所有
    void closeAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& p : channels_) {
            p.second->stop();
        }
        channels_.clear();
    }

private:
    ChannelManager() = default;
    ~ChannelManager() { closeAll(); }
    ChannelManager(const ChannelManager&) = delete;
    ChannelManager& operator=(const ChannelManager&) = delete;
    
    std::map<std::string, std::shared_ptr<Channel>> channels_;
    mutable std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_CHANNEL_HPP
