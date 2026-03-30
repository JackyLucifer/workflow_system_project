#include "workflow_system/plugin/communication/MessageBus.hpp"
#include "workflow_system/core/logger.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace WorkflowSystem { namespace Plugin {

MessageBus::MessageBus()
    : MessageBus(std::thread::hardware_concurrency() > 0 ?
                 std::thread::hardware_concurrency() : 4) {}

MessageBus::MessageBus(size_t threadCount)
    : threadCount_(threadCount) {
}

MessageBus::~MessageBus() {
    stopBackgroundProcessing();
}

bool MessageBus::registerHandler(const std::string& pluginId,
                                const std::string& method,
                                MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    HandlerKey key{pluginId, method};
    auto registration = std::make_shared<MessageHandlerRegistration>(
        generateHandlerId(), pluginId, method, handler
    );

    handlers_[key] = registration;

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalHandlers++;
    }

    LOG_INFO("注册消息处理器: " + pluginId + "::" + method);

    return true;
}

bool MessageBus::registerAsyncHandler(const std::string& pluginId,
                                     const std::string& method,
                                     AsyncMessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    HandlerKey key{pluginId, method};
    auto registration = std::make_shared<MessageHandlerRegistration>(
        generateHandlerId(), pluginId, method, handler
    );

    handlers_[key] = registration;

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalHandlers++;
    }

    LOG_INFO("注册异步消息处理器: " + pluginId + "::" + method);

    return true;
}

bool MessageBus::unregisterHandler(const std::string& pluginId,
                                  const std::string& method) {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    HandlerKey key{pluginId, method};
    auto it = handlers_.find(key);

    if (it == handlers_.end()) {
        return false;
    }

    handlers_.erase(it);

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalHandlers--;
    }

    LOG_INFO("注销消息处理器: " + pluginId + "::" + method);

    return true;
}

size_t MessageBus::unregisterAllHandlers(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    size_t count = 0;

    for (auto it = handlers_.begin(); it != handlers_.end();) {
        if (it->first.first == pluginId) {
            it = handlers_.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalHandlers -= count;
    }

    LOG_INFO("注销插件的所有消息处理器: " + pluginId + " 数量: " + std::to_string(count));

    return count;
}

Response MessageBus::sendMessage(const Message& message, uint64_t timeoutMs) {
    // 如果消息是通知类型，直接发送
    if (message.isNotification()) {
        sendNotification(message);
        return Response::ok("Notification sent");
    }

    // 如果是响应类型，不需要处理
    if (message.isResponse()) {
        return Response::error("Cannot send a response message");
    }

    // 处理消息
    auto response = processMessage(message);

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalMessagesSent++;
    }

    return response;
}

void MessageBus::sendNotification(const Message& message) {
    {
        std::lock_guard<std::mutex> lock(notificationMutex_);
        notificationQueue_.push(message);
    }

    notificationCondition_.notify_one();

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalNotificationsSent++;
    }
}

void MessageBus::broadcast(const Message& message) {
    std::vector<std::shared_ptr<MessageHandlerRegistration>> targets;

    {
        std::lock_guard<std::mutex> lock(handlersMutex_);

        for (auto& pair : handlers_) {
            if (pair.second->active) {
                targets.push_back(pair.second);
            }
        }
    }

    for (auto& registration : targets) {
        Message broadcastMsg = message;
        broadcastMsg.header().to = registration->pluginId;

        sendNotification(broadcastMsg);
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.totalBroadcasts++;
    }
}

std::future<Response> MessageBus::sendMessageAsync(const Message& message,
                                                   uint64_t timeoutMs) {
    auto promise = std::make_shared<std::promise<Response>>();
    auto future = promise->get_future();

    // 创建待处理消息
    PendingMessage pending;
    pending.message = message;
    pending.responsePromise = promise;
    pending.deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timeoutMs);

    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingMessages_.push(pending);
    }

    pendingCondition_.notify_one();

    return future;
}

Response MessageBus::handleMessage(const Message& message) {
    return processMessage(message);
}

size_t MessageBus::getHandlerCount() const {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    return handlers_.size();
}

std::vector<std::string> MessageBus::getPluginMethods(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    std::vector<std::string> methods;

    for (const auto& pair : handlers_) {
        if (pair.first.first == pluginId) {
            methods.push_back(pair.first.second);
        }
    }

    return methods;
}

void MessageBus::startBackgroundProcessing() {
    if (running_.load()) {
        return;
    }

    running_.store(true);

    // 启动工作线程处理待处理消息
    for (size_t i = 0; i < threadCount_; ++i) {
        workerThreads_.emplace_back([this]() {
            while (running_.load()) {
                PendingMessage pending;

                {
                    std::unique_lock<std::mutex> lock(pendingMutex_);
                    pendingCondition_.wait(lock, [this]() {
                        return !pendingMessages_.empty() || !running_.load();
                    });

                    if (!running_.load()) {
                        break;
                    }

                    if (pendingMessages_.empty()) {
                        continue;
                    }

                    pending = pendingMessages_.front();
                    pendingMessages_.pop();
                }

                // 检查是否超时
                if (pending.isExpired()) {
                    pending.responsePromise->set_value(
                        Response::error("Message timeout")
                    );

                    {
                        std::lock_guard<std::mutex> statsLock(statsMutex_);
                        statistics_.timeoutMessages++;
                    }
                    continue;
                }

                // 处理消息
                try {
                    auto response = processMessage(pending.message);
                    pending.responsePromise->set_value(response);
                } catch (const std::exception& e) {
                    pending.responsePromise->set_value(
                        Response::error(std::string("Processing error: ") + e.what())
                    );

                    {
                        std::lock_guard<std::mutex> statsLock(statsMutex_);
                        statistics_.failedMessages++;
                    }
                }
            }
        });
    }

    // 启动通知处理线程
    notificationThread_ = std::thread([this]() {
        processNotificationQueue();
    });

    // 启动超时检测线程
    timeoutThread_ = std::thread([this]() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            checkTimeouts();
        }
    });

    LOG_INFO("消息总线后台处理已启动，线程数: " + std::to_string(threadCount_));
}

void MessageBus::stopBackgroundProcessing() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // 通知所有线程
    pendingCondition_.notify_all();
    notificationCondition_.notify_all();

    // 等待工作线程完成
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // 等待通知线程
    if (notificationThread_.joinable()) {
        notificationThread_.join();
    }

    // 等待超时检测线程
    if (timeoutThread_.joinable()) {
        timeoutThread_.join();
    }

    workerThreads_.clear();
    LOG_INFO("消息总线后台处理已停止");
}

void MessageBus::waitForCompletion() {
    std::unique_lock<std::mutex> pendingLock(pendingMutex_);
    std::unique_lock<std::mutex> notificationLock(notificationMutex_);

    pendingCondition_.wait(pendingLock, [this]() {
        return pendingMessages_.empty() || !running_.load();
    });

    notificationCondition_.wait(notificationLock, [this]() {
        return notificationQueue_.empty() || !running_.load();
    });
}

MessageBus::Statistics MessageBus::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    Statistics stats = statistics_;
    stats.pendingMessages = pendingMessages_.size();

    return stats;
}

void MessageBus::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_ = Statistics{};
}

Response MessageBus::processMessage(const Message& message) {
    std::shared_ptr<MessageHandlerRegistration> registration;

    {
        std::lock_guard<std::mutex> lock(handlersMutex_);

        HandlerKey key{message.to(), message.method()};
        auto it = handlers_.find(key);

        if (it == handlers_.end() || !it->second->active) {
            return Response::error("No handler found for: " + message.to() + "::" + message.method());
        }

        registration = it->second;
    }

    try {
        // 检查是否有异步处理器
        if (registration->asyncHandler) {
            Response response;
            std::exception_ptr exception;

            // 使用同步方式调用异步处理器
            registration->asyncHandler(message, [&response, &exception](const Response& r) {
                response = r;
            });

            // 等待异步处理完成（简单实现）
            // 注意：这里可以改进为更优雅的异步处理
            return response;
        } else {
            return registration->handler(message);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("消息处理器异常: " + std::string(e.what()));
        return Response::error(std::string("Handler error: ") + e.what());
    }
}

void MessageBus::processNotificationQueue() {
    while (running_.load()) {
        Message message;

        {
            std::unique_lock<std::mutex> lock(notificationMutex_);
            notificationCondition_.wait(lock, [this]() {
                return !notificationQueue_.empty() || !running_.load();
            });

            if (!running_.load()) {
                break;
            }

            if (notificationQueue_.empty()) {
                continue;
            }

            message = notificationQueue_.front();
            notificationQueue_.pop();
        }

        // 处理通知（不需要响应）
        try {
            processMessage(message);
        } catch (const std::exception& e) {
            LOG_ERROR("通知处理异常: " + std::string(e.what()));
        }
    }
}

std::string MessageBus::generateHandlerId() {
    static std::atomic<uint64_t> counter{1};
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 16; ++i) {
        oss << std::setw(1) << std::setfill('0') << dis(gen);
    }
    oss << "-" << counter++;

    return oss.str();
}

void MessageBus::checkTimeouts() {
    std::lock_guard<std::mutex> lock(pendingMutex_);

    // 由于queue不支持遍历，我们需要重新组织
    // 这里使用一个临时队列来保存未超时的消息
    std::queue<PendingMessage> remaining;
    size_t timeoutCount = 0;

    while (!pendingMessages_.empty()) {
        auto pending = pendingMessages_.front();
        pendingMessages_.pop();

        if (pending.isExpired()) {
            pending.responsePromise->set_value(
                Response::error("Message timeout")
            );
            timeoutCount++;
        } else {
            remaining.push(pending);
        }
    }

    pendingMessages_.swap(remaining);

    if (timeoutCount > 0) {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.timeoutMessages += timeoutCount;
    }
}

} // namespace Plugin
} // namespace WorkflowSystem
