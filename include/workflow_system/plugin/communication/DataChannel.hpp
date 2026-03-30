#ifndef PLUGIN_FRAMEWORK_DATA_CHANNEL_HPP
#define PLUGIN_FRAMEWORK_DATA_CHANNEL_HPP

#include "../core/IPluginContext.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <atomic>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 基于内存的数据通道
 */
class MemoryDataChannel : public IDataChannel {
public:
    explicit MemoryDataChannel(const std::string& channelId,
                              size_t maxSize = 1024 * 1024)  // 1MB默认
        : channelId_(channelId)
        , maxSize_(maxSize)
        , currentSize_(0)
        , closed_(false) {}

    ~MemoryDataChannel() override {
        close();
    }

    void send(const std::vector<uint8_t>& data) override {
        if (closed_.load()) {
            throw std::runtime_error("Channel is closed");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 检查容量
        if (currentSize_ + data.size() > maxSize_) {
            // 可以选择阻塞或丢弃旧数据
            // 这里选择丢弃最旧的数据
            while (currentSize_ + data.size() > maxSize_ && !queue_.empty()) {
                auto& front = queue_.front();
                currentSize_ -= front.size();
                queue_.pop();
            }
        }

        queue_.push(data);
        currentSize_ += data.size();
        condition_.notify_one();
    }

    std::vector<uint8_t> receive() override {
        std::unique_lock<std::mutex> lock(mutex_);

        condition_.wait(lock, [this]() {
            return !queue_.empty() || closed_.load();
        });

        if (closed_.load() && queue_.empty()) {
            return {};
        }

        auto data = queue_.front();
        queue_.pop();
        currentSize_ -= data.size();

        return data;
    }

    std::vector<std::vector<uint8_t>> receiveBatch(size_t count,
                                                    Duration timeout) override {
        std::vector<std::vector<uint8_t>> result;
        result.reserve(count);

        std::unique_lock<std::mutex> lock(mutex_);

        auto deadline = std::chrono::system_clock::now() +
                       std::chrono::milliseconds(timeout.getMilliseconds());

        while (result.size() < count) {
            condition_.wait_until(lock, deadline, [this]() {
                return !queue_.empty() || closed_.load();
            });

            if (closed_.load() && queue_.empty()) {
                break;
            }

            if (queue_.empty()) {
                break;  // 超时
            }

            auto data = queue_.front();
            queue_.pop();
            currentSize_ -= data.size();
            result.push_back(data);
        }

        return result;
    }

    std::string getId() const override {
        return channelId_;
    }

    void close() override {
        closed_.store(true);
        condition_.notify_all();
    }

    // 额外功能
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    size_t dataSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentSize_;
    }

    bool isClosed() const {
        return closed_.load();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        currentSize_ = 0;
    }

private:
    std::string channelId_;
    size_t maxSize_;
    std::atomic<size_t> currentSize_;

    std::queue<std::vector<uint8_t>> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> closed_;
};

/**
 * @brief 数据通道管理器
 */
class DataChannelManager {
public:
    static DataChannelManager& getInstance() {
        static DataChannelManager instance;
        return instance;
    }

    /**
     * @brief 创建数据通道
     */
    IDataChannel* createChannel(const std::string& channelId,
                               size_t maxSize = 1024 * 1024) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto channel = std::unique_ptr<MemoryDataChannel>(new MemoryDataChannel(channelId, maxSize));
        auto* ptr = channel.get();

        channels_[channelId] = std::move(channel);

        return ptr;
    }

    /**
     * @brief 获取数据通道
     */
    IDataChannel* getChannel(const std::string& channelId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = channels_.find(channelId);
        return (it != channels_.end()) ? it->second.get() : nullptr;
    }

    /**
     * @brief 关闭数据通道
     */
    void closeChannel(const std::string& channelId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = channels_.find(channelId);
        if (it != channels_.end()) {
            it->second->close();
            channels_.erase(it);
        }
    }

    /**
     * @brief 关闭所有通道
     */
    void closeAll() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& pair : channels_) {
            pair.second->close();
        }

        channels_.clear();
    }

    /**
     * @brief 获取所有通道ID
     */
    std::vector<std::string> getAllChannelIds() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> ids;
        ids.reserve(channels_.size());

        for (const auto& pair : channels_) {
            ids.push_back(pair.first);
        }

        return ids;
    }

    /**
     * @brief 获取通道数量
     */
    size_t getChannelCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return channels_.size();
    }

private:
    DataChannelManager() = default;
    ~DataChannelManager() {
        closeAll();
    }

    // 禁止拷贝
    DataChannelManager(const DataChannelManager&) = delete;
    DataChannelManager& operator=(const DataChannelManager&) = delete;

    std::map<std::string, std::unique_ptr<MemoryDataChannel>> channels_;
    mutable std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_DATA_CHANNEL_HPP
