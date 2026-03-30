#ifndef PLUGIN_FRAMEWORK_EVENT_HPP
#define PLUGIN_FRAMEWORK_EVENT_HPP

#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <vector>
#include <cstdint>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 事件数据（通用类型）
 */
using EventData = std::map<std::string, std::string>;

/**
 * @brief 事件优先级
 */
enum class EventPriority {
    LOW = 0,
    NORMAL = 50,
    HIGH = 100,
    CRITICAL = 200
};

/**
 * @brief 事件基类
 */
class IEvent {
public:
    virtual ~IEvent() = default;

    /**
     * @brief 获取事件名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取事件源插件ID
     */
    virtual std::string getSource() const = 0;

    /**
     * @brief 获取事件目标插件ID（空表示广播）
     */
    virtual std::string getTarget() const = 0;

    /**
     * @brief 获取事件数据
     */
    virtual const EventData& getData() const = 0;

    /**
     * @brief 获取事件时间戳
     */
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;

    /**
     * @brief 获取事件优先级
     */
    virtual EventPriority getPriority() const = 0;

    /**
     * @brief 是否应该传播（返回false停止传播）
     */
    virtual bool shouldPropagate() const { return true; }

    /**
     * @brief 是否同步处理
     */
    virtual bool isSynchronous() const { return true; }
};

/**
 * @brief 事件实现
 */
class Event : public IEvent {
public:
    Event(const std::string& name,
          const std::string& source,
          const EventData& data = {},
          const std::string& target = "",
          EventPriority priority = EventPriority::NORMAL)
        : name_(name)
        , source_(source)
        , target_(target)
        , data_(data)
        , priority_(priority)
        , timestamp_(std::chrono::system_clock::now()) {}

    std::string getName() const override { return name_; }
    std::string getSource() const override { return source_; }
    std::string getTarget() const override { return target_; }
    const EventData& getData() const override { return data_; }
    std::chrono::system_clock::time_point getTimestamp() const override { return timestamp_; }
    EventPriority getPriority() const override { return priority_; }

    /**
     * @brief 设置是否传播
     */
    void setPropagate(bool propagate) { propagate_ = propagate; }
    bool shouldPropagate() const override { return propagate_; }

    /**
     * @brief 设置是否同步
     */
    void setSynchronous(bool sync) { synchronous_ = sync; }
    bool isSynchronous() const override { return synchronous_; }

    /**
     * @brief 获取数据值（整数）
     */
    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    /**
     * @brief 获取数据值（浮点数）
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::stod(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    /**
     * @brief 获取数据值（布尔值）
     */
    bool getBool(const std::string& key, bool defaultValue = false) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second == "true" || it->second == "1";
        }
        return defaultValue;
    }
    
    /**
     * @brief 获取数据值（字符串）
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : defaultValue;
    }

private:
    std::string name_;
    std::string source_;
    std::string target_;
    EventData data_;
    EventPriority priority_;
    std::chrono::system_clock::time_point timestamp_;
    bool propagate_ = true;
    bool synchronous_ = true;
};

/**
 * @brief 订阅ID
 */
using SubscriptionId = uint64_t;

/**
 * @brief 事件处理器
 */
using EventHandler = std::function<void(const IEvent&)>;

/**
 * @brief 订阅信息
 */
struct Subscription {
    SubscriptionId id;
    std::string subscriberId;
    std::string eventName;
    EventHandler handler;
    EventPriority priority;
    bool once = false;                // 是否只触发一次
    bool active = true;               // 是否激活

    Subscription() : id(0) {}

    Subscription(SubscriptionId i, const std::string& sub,
               const std::string& name, const EventHandler& h,
               EventPriority p = EventPriority::NORMAL)
        : id(i), subscriberId(sub), eventName(name)
        , handler(h), priority(p) {}
};

/**
 * @brief 事件过滤器
 */
using EventFilter = std::function<bool(const IEvent&)>;

/**
 * @brief 事件结果
 */
struct EventResult {
    size_t subscriberCount = 0;      // 通知的订阅者数量
    size_t successCount = 0;          // 成功处理数量
    size_t failureCount = 0;          // 失败处理数量
    std::vector<std::string> errors;  // 错误信息

    bool isSuccess() const { return failureCount == 0; }
};

/**
 * @brief 预定义的事件名称
 */
namespace Events {
    // 生命周期事件
    constexpr const char* PLUGIN_LOADED = "plugin.loaded";
    constexpr const char* PLUGIN_UNLOADED = "plugin.unloaded";
    constexpr const char* PLUGIN_ENABLED = "plugin.enabled";
    constexpr const char* PLUGIN_DISABLED = "plugin.disabled";
    constexpr const char* PLUGIN_ERROR = "plugin.error";

    // 系统事件
    constexpr const char* SYSTEM_STARTUP = "system.startup";
    constexpr const char* SYSTEM_SHUTDOWN = "system.shutdown";
    constexpr const char* SYSTEM_CONFIG_CHANGED = "system.config_changed";

    // 数据事件
    constexpr const char* DATA_RECEIVED = "data.received";
    constexpr const char* DATA_PROCESSED = "data.processed";
    constexpr const char* DATA_ERROR = "data.error";

    // 通信事件
    constexpr const char* MESSAGE_RECEIVED = "message.received";
    constexpr const char* MESSAGE_SENT = "message.sent";
    constexpr const char* MESSAGE_FAILED = "message.failed";
}

/**
 * @brief 事件构建器
 */
class EventBuilder {
public:
    EventBuilder(const std::string& name, const std::string& source)
        : event_(name, source) {}

    EventBuilder& setTarget(const std::string& target) {
        event_ = Event(event_.getName(), event_.getSource(), {}, target);
        return *this;
    }

    EventBuilder& setPriority(EventPriority priority) {
        event_ = Event(event_.getName(), event_.getSource(),
                     event_.getData(), event_.getTarget(), priority);
        return *this;
    }

    EventBuilder& setData(const std::string& key, const std::string& value) {
        auto data = event_.getData();
        data[key] = value;
        event_ = Event(event_.getName(), event_.getSource(), data,
                     event_.getTarget(), event_.getPriority());
        return *this;
    }

    EventBuilder& setData(const EventData& data) {
        event_ = Event(event_.getName(), event_.getSource(), data,
                     event_.getTarget(), event_.getPriority());
        return *this;
    }

    EventBuilder& setAsync(bool async = true) {
        event_.setSynchronous(!async);
        return *this;
    }

    Event build() const { return event_; }

private:
    Event event_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_EVENT_HPP
