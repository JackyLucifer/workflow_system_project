#include "workflow_system/plugin/runtime/PluginContext.hpp"
#include "workflow_system/plugin/core/IPluginManager.hpp"
#include "workflow_system/plugin/utils/Logger.hpp"
#include "workflow_system/plugin/communication/EventBus.hpp"
#include "workflow_system/plugin/communication/MessageBus.hpp"
#include "workflow_system/plugin/communication/DataChannel.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <algorithm>

namespace WorkflowSystem { namespace Plugin {

// 简单的线程池实现
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 4)
        : running_(true) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this]() {
                while (running_.load()) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this]() {
                            return !tasks_.empty() || !running_.load();
                        });

                        if (!running_.load()) {
                            break;
                        }

                        if (tasks_.empty()) {
                            continue;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        running_.store(false);
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void execute(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            tasks_.push(std::move(task));
        }
        condition_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> running_;
};

// 简单的定时器服务
class TimerService {
public:
    TimerService() : running_(false), nextId_(1) {}

    ~TimerService() {
        stop();
    }

    void start() {
        if (running_.load()) {
            return;
        }

        running_.store(true);
        worker_ = std::thread([this]() {
            run();
        });
    }

    void stop() {
        if (!running_.load()) {
            return;
        }

        running_.store(false);
        condition_.notify_all();

        if (worker_.joinable()) {
            worker_.join();
        }
    }

    TimerId scheduleDelayed(uint64_t delayMs, std::function<void()> callback) {
        auto id = nextId_++;

        TimerEntry entry;
        entry.id = id;
        entry.callback = callback;
        entry.nextRun = std::chrono::system_clock::now() + std::chrono::milliseconds(delayMs);
        entry.interval = 0;
        entry.periodic = false;

        {
            std::lock_guard<std::mutex> lock(timersMutex_);
            timers_[id] = entry;
        }

        condition_.notify_one();
        return id;
    }

    TimerId scheduleInterval(uint64_t intervalMs, std::function<void()> callback) {
        auto id = nextId_++;

        TimerEntry entry;
        entry.id = id;
        entry.callback = callback;
        entry.nextRun = std::chrono::system_clock::now() + std::chrono::milliseconds(intervalMs);
        entry.interval = intervalMs;
        entry.periodic = true;

        {
            std::lock_guard<std::mutex> lock(timersMutex_);
            timers_[id] = entry;
        }

        condition_.notify_one();
        return id;
    }

    void cancel(TimerId id) {
        std::lock_guard<std::mutex> lock(timersMutex_);
        timers_.erase(id);
    }

private:
    void run() {
        while (running_.load()) {
            std::vector<TimerEntry> readyTimers;

            {
                std::unique_lock<std::mutex> lock(timersMutex_);

                auto now = std::chrono::system_clock::now();
                auto nextTime = now + std::chrono::hours(24);

                // 查找准备好的定时器
                for (auto it = timers_.begin(); it != timers_.end();) {
                    auto& entry = it->second;

                    if (entry.nextRun <= now) {
                        readyTimers.push_back(entry);

                        if (entry.periodic) {
                            entry.nextRun = now + std::chrono::milliseconds(entry.interval);
                            ++it;
                        } else {
                            it = timers_.erase(it);
                        }
                    } else {
                        if (entry.nextRun < nextTime) {
                            nextTime = entry.nextRun;
                        }
                        ++it;
                    }
                }

                // 等待到下一个定时器
                if (readyTimers.empty() && !timers_.empty()) {
                    condition_.wait_until(lock, nextTime);
                } else if (timers_.empty()) {
                    condition_.wait(lock);
                }
            }

            // 执行回调
            for (auto& entry : readyTimers) {
                try {
                    entry.callback();
                } catch (...) {
                    // 忽略异常
                }
            }
        }
    }

    struct TimerEntry {
        TimerId id;
        std::function<void()> callback;
        std::chrono::system_clock::time_point nextRun;
        uint64_t interval;
        bool periodic;
    };

    std::thread worker_;
    std::map<TimerId, TimerEntry> timers_;
    std::mutex timersMutex_;
    std::condition_variable condition_;
    std::atomic<bool> running_;
    std::atomic<TimerId> nextId_;
};

PluginContext::PluginContext(const std::string& pluginId,
                            const PluginSpec& spec,
                            IPluginManager* manager,
                            IEventBus* eventBus,
                            IMessageBus* messageBus)
    : pluginId_(pluginId)
    , spec_(spec)
    , state_(PluginState::LOADED)
    , manager_(manager)
    , eventBus_(eventBus)
    , messageBus_(messageBus) {

    // 设置目录
    pluginDirectory_ = "./plugins/" + pluginId;
    dataDirectory_ = "./data/" + pluginId;
    configDirectory_ = "./config/" + pluginId;
    logDirectory_ = "./logs/" + pluginId;

    // 创建日志器
    logger_ = &Logger::getLogger(pluginId);

    threadPool_ = std::unique_ptr<ThreadPool>(new ThreadPool(4));
}

PluginContext::~PluginContext() {
    shutdown();
}

// ==================== 插件信息 ====================

std::string PluginContext::getPluginId() const {
    return pluginId_;
}

PluginSpec PluginContext::getSpec() const {
    return spec_;
}

PluginState PluginContext::getState() const {
    return state_;
}

std::string PluginContext::getPluginDirectory() const {
    return pluginDirectory_;
}

std::string PluginContext::getDataDirectory() const {
    return dataDirectory_;
}

std::string PluginContext::getConfigDirectory() const {
    return configDirectory_;
}

std::string PluginContext::getLogDirectory() const {
    return logDirectory_;
}

// ==================== 配置管理 ====================

std::string PluginContext::getConfig(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex_);

    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : "";
}

void PluginContext::setConfig(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_[key] = value;
}

std::string PluginContext::getAllConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);

    std::ostringstream oss;
    oss << "{";

    bool first = true;
    for (const auto& pair : config_) {
        if (!first) {
            oss << ",";
        }
        oss << "\"" << pair.first << "\":\"" << pair.second << "\"";
        first = false;
    }

    oss << "}";
    return oss.str();
}

bool PluginContext::saveConfig() {
    std::lock_guard<std::mutex> lock(configMutex_);

    std::string configFile = configDirectory_ + "/config.json";
    std::ofstream file(configFile);

    if (!file.is_open()) {
        return false;
    }

    file << getAllConfig();
    return true;
}

bool PluginContext::reloadConfig() {
    std::lock_guard<std::mutex> lock(configMutex_);

    std::string configFile = configDirectory_ + "/config.json";
    std::ifstream file(configFile);

    if (!file.is_open()) {
        return false;
    }

    // 简单实现：实际应该解析JSON
    return true;
}

// ==================== 日志系统 ====================

Logger& PluginContext::getLogger() {
    return *logger_;
}

void PluginContext::logDebug(const std::string& message) {
    logger_->debug(message);
}

void PluginContext::logInfo(const std::string& message) {
    logger_->info(message);
}

void PluginContext::logWarning(const std::string& message) {
    logger_->warning(message);
}

void PluginContext::logError(const std::string& message) {
    logger_->error(message);
}

// ==================== 事件系统 ====================

SubscriptionId PluginContext::subscribe(const std::string& event,
                                       EventHandler handler) {
    return subscribe(event, handler, EventPriority::NORMAL);
}

SubscriptionId PluginContext::subscribe(const std::string& event,
                                       EventHandler handler,
                                       EventPriority priority) {
    if (!eventBus_) {
        return 0;
    }

    auto id = eventBus_->subscribe(pluginId_, event, handler, priority);

    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    subscriptions_.push_back(id);

    return id;
}

void PluginContext::unsubscribe(SubscriptionId subscriptionId) {
    if (!eventBus_) {
        return;
    }

    eventBus_->unsubscribe(subscriptionId);

    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    subscriptions_.erase(
        std::remove(subscriptions_.begin(), subscriptions_.end(), subscriptionId),
        subscriptions_.end()
    );
}

void PluginContext::unsubscribeAll() {
    if (!eventBus_) {
        return;
    }

    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    for (auto id : subscriptions_) {
        eventBus_->unsubscribe(id);
    }
    subscriptions_.clear();
}

EventResult PluginContext::publish(const Event& event) {
    if (!eventBus_) {
        return EventResult{};
    }

    return eventBus_->publish(event);
}

void PluginContext::publishAsync(const Event& event) {
    if (!eventBus_) {
        return;
    }

    eventBus_->publishAsync(event);
}

EventResult PluginContext::publish(const std::string& eventName,
                                  const EventData& data) {
    Event event(eventName, pluginId_, data);
    return publish(event);
}

void PluginContext::publishAsync(const std::string& eventName,
                                const EventData& data) {
    Event event(eventName, pluginId_, data);
    publishAsync(event);
}

// ==================== 消息系统 ====================

Response PluginContext::sendMessage(const std::string& to,
                                   const Message& message,
                                   Duration timeout) {
    if (!messageBus_) {
        return Response::error("Message bus not available");
    }

    return messageBus_->sendMessage(message, timeout.getMilliseconds());
}

void PluginContext::sendNotification(const std::string& to,
                                    const Message& message) {
    if (!messageBus_) {
        return;
    }

    messageBus_->sendNotification(message);
}

void PluginContext::broadcast(const Message& message) {
    if (!messageBus_) {
        return;
    }

    messageBus_->broadcast(message);
}

std::string PluginContext::callRpc(const std::string& pluginId,
                                  const std::string& method,
                                  const std::string& params,
                                  Duration timeout) {
    if (!messageBus_) {
        return "{\"error\": \"Message bus not available\"}";
    }

    Message message(pluginId_, pluginId, method, MessageType::REQUEST);

    auto response = messageBus_->sendMessage(message, timeout.getMilliseconds());

    if (response.isSuccess()) {
        return response.message();
    } else {
        return "{\"error\": \"" + response.message() + "\"}";
    }
}

// ==================== 插件发现 ====================

IPluginManager* PluginContext::getPluginManager() {
    return manager_;
}

IPlugin* PluginContext::getPlugin(const std::string& pluginId) const {
    if (!manager_) {
        return nullptr;
    }

    return manager_->getPlugin(pluginId);
}

std::vector<IPlugin*> PluginContext::getAllPlugins() const {
    if (!manager_) {
        return {};
    }

    return manager_->getAllPlugins();
}

bool PluginContext::hasPlugin(const std::string& pluginId) const {
    return getPlugin(pluginId) != nullptr;
}

bool PluginContext::isPluginLoaded(const std::string& pluginId) const {
    if (!manager_) {
        return false;
    }

    return manager_->isPluginLoaded(pluginId);
}

// ==================== 数据通道 ====================

IDataChannel* PluginContext::createDataChannel(const std::string& channelId) {
    auto& manager = DataChannelManager::getInstance();
    auto* channel = manager.createChannel(pluginId_ + "." + channelId);

    {
        std::lock_guard<std::mutex> lock(dataChannelsMutex_);
        dataChannels_[channelId] = channel;
    }

    return channel;
}

IDataChannel* PluginContext::getDataChannel(const std::string& channelId) {
    std::lock_guard<std::mutex> lock(dataChannelsMutex_);

    auto it = dataChannels_.find(channelId);
    return (it != dataChannels_.end()) ? it->second : nullptr;
}

void PluginContext::closeDataChannel(const std::string& channelId) {
    auto& manager = DataChannelManager::getInstance();
    manager.closeChannel(pluginId_ + "." + channelId);

    {
        std::lock_guard<std::mutex> lock(dataChannelsMutex_);
        dataChannels_.erase(channelId);
    }
}

// ==================== 资源管理 ====================

void PluginContext::registerResource(const std::string& resourceId,
                                    void* resource,
                                    std::function<void(void*)> deleter) {
    std::lock_guard<std::mutex> lock(resourcesMutex_);

    ResourceEntry entry;
    entry.resource = resource;
    entry.deleter = deleter;

    resources_[resourceId] = entry;
}

void* PluginContext::getResource(const std::string& resourceId) const {
    std::lock_guard<std::mutex> lock(resourcesMutex_);

    auto it = resources_.find(resourceId);
    return (it != resources_.end()) ? it->second.resource : nullptr;
}

void PluginContext::unregisterResource(const std::string& resourceId) {
    std::lock_guard<std::mutex> lock(resourcesMutex_);

    auto it = resources_.find(resourceId);
    if (it != resources_.end()) {
        if (it->second.deleter && it->second.resource) {
            it->second.deleter(it->second.resource);
        }
        resources_.erase(it);
    }
}

// ==================== 定时器 ====================

TimerId PluginContext::scheduleDelayed(Duration delay, Callback callback) {
    // 简化实现：实际应该使用TimerService
    // 这里返回一个占位符ID
    return nextTimerId_++;
}

TimerId PluginContext::scheduleInterval(Duration interval, Callback callback) {
    // 简化实现
    return nextTimerId_++;
}

void PluginContext::cancelTimer(TimerId timerId) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    timers_.erase(timerId);
}

// ==================== 线程池 ====================

void PluginContext::executeAsync(Callback task) {
    if (threadPool_) {
        threadPool_->execute(task);
    }
}

void PluginContext::executeAsync(Callback task, Callback onComplete) {
    if (threadPool_) {
        threadPool_->execute([task, onComplete]() {
            task();
            if (onComplete) {
                onComplete();
            }
        });
    }
}

ThreadPool* PluginContext::getThreadPool() {
    return threadPool_.get();
}

// ==================== 生命周期控制 ====================

bool PluginContext::requestRestart() {
    // 通知管理器重启插件
    return manager_ ? manager_->reloadPlugin(pluginId_).success : false;
}

bool PluginContext::requestReloadConfig() {
    return reloadConfig();
}

bool PluginContext::requestDisable() {
    return manager_ ? manager_->disablePlugin(pluginId_) : false;
}

// ==================== 通讯通道 ====================

std::shared_ptr<Channel> PluginContext::getChannel(const std::string& name) {
    return ChannelManager::instance().get(name);
}

std::shared_ptr<Channel> PluginContext::getDefaultChannel() {
    return ChannelManager::instance().get("default");
}

SubscriptionId PluginContext::subscribeData(const std::string& topic, DataCallback callback) {
    auto channel = getDefaultChannel();
    if (!channel) {
        return 0;
    }
    return channel->subscribe(topic, callback);
}

void PluginContext::unsubscribeData(SubscriptionId id) {
    auto channel = getDefaultChannel();
    if (channel) {
        channel->unsubscribe(id);
    }
}

void PluginContext::publishData(const std::string& topic, 
                                  const std::map<std::string, std::string>& data) {
    auto channel = getDefaultChannel();
    if (channel) {
        channel->publish(topic, pluginId_, data);
    }
}

void PluginContext::publishDataSync(const std::string& topic,
                                      const std::map<std::string, std::string>& data) {
    auto channel = getDefaultChannel();
    if (channel) {
                DataPacket packet;
                packet.topic = topic;
                packet.source = pluginId_;
                packet.data = data;
                channel->publishSync(packet);
    }
}

// ==================== 系统服务 ====================

void* PluginContext::getService(const std::string& serviceName) const {
    std::lock_guard<std::mutex> lock(servicesMutex_);

    auto it = services_.find(serviceName);
    return (it != services_.end()) ? it->second : nullptr;
}

void PluginContext::registerService(const std::string& serviceName, void* service) {
    std::lock_guard<std::mutex> lock(servicesMutex_);
    services_[serviceName] = service;
}

// ==================== 内部方法 ====================

void PluginContext::setState(PluginState state) {
    state_ = state;
}

void PluginContext::initialize() {
    // 创建目录
    // 实际实现中应该使用filesystem或系统调用
}

void PluginContext::shutdown() {
    // 取消所有订阅
    unsubscribeAll();

    // 关闭所有数据通道
    {
        std::lock_guard<std::mutex> lock(dataChannelsMutex_);
        for (auto& pair : dataChannels_) {
            pair.second->close();
        }
        dataChannels_.clear();
    }

    // 清理资源
    {
        std::lock_guard<std::mutex> lock(resourcesMutex_);
        for (auto& pair : resources_) {
            if (pair.second.deleter && pair.second.resource) {
                pair.second.deleter(pair.second.resource);
            }
        }
        resources_.clear();
    }
}

} // namespace Plugin
} // namespace WorkflowSystem
