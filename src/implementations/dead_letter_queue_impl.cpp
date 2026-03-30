/**
 * @file dead_letter_queue_impl.cpp
 * @brief 死信队列实现
 */

#include "workflow_system/implementations/dead_letter_queue_impl.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"
#include <algorithm>
#include <climits>

namespace WorkflowSystem {

DeadLetterQueue::DeadLetterQueue() : config_() {}

// ========== 入队操作 ==========

std::string DeadLetterQueue::enqueue(const DeadLetterItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string itemId = item.itemId;
    if (itemId.empty()) {
        itemId = "dlq_" + IdGenerator::generate();
    }

    DeadLetterItem newItem = item;
    newItem.itemId = itemId;
    newItem.createdAt = TimeUtils::getCurrentMs();
    newItem.status = DeadLetterStatus::PENDING;

    items_[itemId] = newItem;
    pendingQueue_.push_back(itemId);

    // 检查容量限制
    while (items_.size() > static_cast<size_t>(config_.maxItems)) {
        removeOldestItem();
    }

    LOG_INFO("[DeadLetterQueue] Enqueued item: " + itemId +
             " reason: " + deadLetterReasonToString(item.reason));

    if (enqueueCallback_) {
        enqueueCallback_(newItem);
    }

    return itemId;
}

std::string DeadLetterQueue::enqueueFromError(const std::string& workflowName,
                                const std::string& errorMessage,
                                DeadLetterReason reason) {
    DeadLetterItem item;
    item.workflowName = workflowName;
    item.errorMessage = errorMessage;
    item.reason = reason;
    item.maxRetries = config_.maxRetries;

    return enqueue(item);
}

// ========== 出队和查询 ==========

DeadLetterItem DeadLetterQueue::getItem(const std::string& itemId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(itemId);
    if (it != items_.end()) {
        return it->second;
    }
    return DeadLetterItem();
}

std::vector<DeadLetterItem> DeadLetterQueue::getPendingItems() const {
    return getItemsByStatus(DeadLetterStatus::PENDING, 1000);
}

std::vector<DeadLetterItem> DeadLetterQueue::getItemsByStatus(DeadLetterStatus status, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<DeadLetterItem> result;
    for (const auto& pair : items_) {
        if (pair.second.status == status) {
            result.push_back(pair.second);
            if (static_cast<int>(result.size()) >= limit) {
                break;
            }
        }
    }
    return result;
}

std::vector<DeadLetterItem> DeadLetterQueue::getItemsByWorkflow(const std::string& workflowName) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<DeadLetterItem> result;
    for (const auto& pair : items_) {
        if (pair.second.workflowName == workflowName) {
            result.push_back(pair.second);
        }
    }
    return result;
}

size_t DeadLetterQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size();
}

bool DeadLetterQueue::isEmpty() const {
    return size() == 0;
}

// ========== 重试操作 ==========

bool DeadLetterQueue::retry(const std::string& itemId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(itemId);
    if (it == items_.end()) {
        return false;
    }

    if (it->second.status != DeadLetterStatus::PENDING &&
        it->second.status != DeadLetterStatus::RETRYING) {
        return false;
    }

    if (it->second.retryCount >= it->second.maxRetries) {
        it->second.status = DeadLetterStatus::DISCARDED;
        LOG_WARNING("[DeadLetterQueue] Max retries exceeded for: " + itemId);
        return false;
    }

    it->second.status = DeadLetterStatus::RETRYING;
    it->second.retryCount++;
    it->second.lastAttemptAt = TimeUtils::getCurrentMs();

    LOG_INFO("[DeadLetterQueue] Retrying item: " + itemId +
             " attempt: " + std::to_string(it->second.retryCount));

    if (retryCallback_) {
        retryCallback_(it->second);
    }

    return true;
}

int DeadLetterQueue::retryBatch(const std::vector<std::string>& itemIds) {
    int successCount = 0;
    for (const auto& itemId : itemIds) {
        if (retry(itemId)) {
            successCount++;
        }
    }
    return successCount;
}

int DeadLetterQueue::retryAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    int successCount = 0;
    for (const auto& itemId : pendingQueue_) {
        auto it = items_.find(itemId);
        if (it != items_.end() &&
            (it->second.status == DeadLetterStatus::PENDING ||
             it->second.status == DeadLetterStatus::RETRYING)) {
            if (it->second.retryCount < it->second.maxRetries) {
                it->second.status = DeadLetterStatus::RETRYING;
                it->second.retryCount++;
                it->second.lastAttemptAt = TimeUtils::getCurrentMs();
                successCount++;

                if (retryCallback_) {
                    retryCallback_(it->second);
                }
            }
        }
    }

    LOG_INFO("[DeadLetterQueue] Retried " + std::to_string(successCount) + " items");
    return successCount;
}

// ========== 解决操作 ==========

bool DeadLetterQueue::resolve(const std::string& itemId,
                const std::string& resolution,
                const std::string& resolvedBy) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(itemId);
    if (it == items_.end()) {
        return false;
    }

    it->second.status = DeadLetterStatus::RESOLVED;
    it->second.resolution = resolution;
    it->second.resolvedBy = resolvedBy;
    it->second.resolvedAt = TimeUtils::getCurrentMs();

    // 从待处理队列移除
    pendingQueue_.erase(
        std::remove(pendingQueue_.begin(), pendingQueue_.end(), itemId),
        pendingQueue_.end());

    LOG_INFO("[DeadLetterQueue] Resolved item: " + itemId +
             " by: " + resolvedBy);

    if (resolveCallback_) {
        resolveCallback_(it->second);
    }

    return true;
}

bool DeadLetterQueue::discard(const std::string& itemId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(itemId);
    if (it == items_.end()) {
        return false;
    }

    it->second.status = DeadLetterStatus::DISCARDED;
    it->second.resolution = "Discarded: " + reason;
    it->second.resolvedAt = TimeUtils::getCurrentMs();

    // 从待处理队列移除
    pendingQueue_.erase(
        std::remove(pendingQueue_.begin(), pendingQueue_.end(), itemId),
        pendingQueue_.end());

    LOG_INFO("[DeadLetterQueue] Discarded item: " + itemId);
    return true;
}

bool DeadLetterQueue::markForManualReview(const std::string& itemId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(itemId);
    if (it == items_.end()) {
        return false;
    }

    it->second.status = DeadLetterStatus::MANUAL_REVIEW;

    LOG_INFO("[DeadLetterQueue] Marked for manual review: " + itemId);
    return true;
}

// ========== 管理操作 ==========

int DeadLetterQueue::clearResolved() {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (auto it = items_.begin(); it != items_.end(); ) {
        if (it->second.status == DeadLetterStatus::RESOLVED ||
            it->second.status == DeadLetterStatus::DISCARDED) {
            it = items_.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    LOG_INFO("[DeadLetterQueue] Cleared " + std::to_string(count) + " resolved items");
    return count;
}

void DeadLetterQueue::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    items_.clear();
    pendingQueue_.clear();

    LOG_INFO("[DeadLetterQueue] Cleared all items");
}

int DeadLetterQueue::clearExpired() {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t now = TimeUtils::getCurrentMs();
    int64_t expirationTime = config_.retentionMs;

    int count = 0;
    for (auto it = items_.begin(); it != items_.end(); ) {
        if (now - it->second.createdAt > expirationTime) {
            pendingQueue_.erase(
                std::remove(pendingQueue_.begin(), pendingQueue_.end(), it->first),
                pendingQueue_.end());
            it = items_.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    LOG_INFO("[DeadLetterQueue] Cleared " + std::to_string(count) + " expired items");
    return count;
}

void DeadLetterQueue::setConfig(const DeadLetterQueueConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

DeadLetterQueueConfig DeadLetterQueue::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

// ========== 回调设置 ==========

void DeadLetterQueue::setEnqueueCallback(DeadLetterCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    enqueueCallback_ = callback;
}

void DeadLetterQueue::setRetryCallback(DeadLetterCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    retryCallback_ = callback;
}

void DeadLetterQueue::setResolveCallback(DeadLetterCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    resolveCallback_ = callback;
}

// ========== 统计 ==========

std::string DeadLetterQueue::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto byStatus = countByStatusInternal();
    auto byReason = countByReasonInternal();

    std::string stats = "{\n";
    stats += "  \"totalItems\": " + std::to_string(items_.size()) + ",\n";
    stats += "  \"pendingItems\": " + std::to_string(byStatus[DeadLetterStatus::PENDING]) + ",\n";
    stats += "  \"byStatus\": {\n";
    stats += "    \"pending\": " + std::to_string(byStatus[DeadLetterStatus::PENDING]) + ",\n";
    stats += "    \"retrying\": " + std::to_string(byStatus[DeadLetterStatus::RETRYING]) + ",\n";
    stats += "    \"resolved\": " + std::to_string(byStatus[DeadLetterStatus::RESOLVED]) + ",\n";
    stats += "    \"discarded\": " + std::to_string(byStatus[DeadLetterStatus::DISCARDED]) + ",\n";
    stats += "    \"manualReview\": " + std::to_string(byStatus[DeadLetterStatus::MANUAL_REVIEW]) + "\n";
    stats += "  },\n";
    stats += "  \"byReason\": {\n";
    stats += "    \"maxRetriesExceeded\": " + std::to_string(byReason[DeadLetterReason::MAX_RETRIES_EXCEEDED]) + ",\n";
    stats += "    \"timeout\": " + std::to_string(byReason[DeadLetterReason::TIMEOUT]) + ",\n";
    stats += "    \"executionError\": " + std::to_string(byReason[DeadLetterReason::EXECUTION_ERROR]) + "\n";
    stats += "  }\n";
    stats += "}";

    return stats;
}

std::unordered_map<DeadLetterReason, int> DeadLetterQueue::countByReason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return countByReasonInternal();
}

std::unordered_map<DeadLetterStatus, int> DeadLetterQueue::countByStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return countByStatusInternal();
}

// ========== 私有方法 ==========

void DeadLetterQueue::removeOldestItem() {
    if (items_.empty()) {
        return;
    }

    // 找到最旧的已解决/已丢弃项
    std::string oldestId;
    int64_t oldestTime = INT64_MAX;

    for (const auto& pair : items_) {
        if (pair.second.status == DeadLetterStatus::RESOLVED ||
            pair.second.status == DeadLetterStatus::DISCARDED) {
            if (pair.second.createdAt < oldestTime) {
                oldestTime = pair.second.createdAt;
                oldestId = pair.first;
            }
        }
    }

    if (!oldestId.empty()) {
        pendingQueue_.erase(
            std::remove(pendingQueue_.begin(), pendingQueue_.end(), oldestId),
            pendingQueue_.end());
        items_.erase(oldestId);
    }
}

std::unordered_map<DeadLetterReason, int> DeadLetterQueue::countByReasonInternal() const {
    std::unordered_map<DeadLetterReason, int> counts;
    for (const auto& pair : items_) {
        counts[pair.second.reason]++;
    }
    return counts;
}

std::unordered_map<DeadLetterStatus, int> DeadLetterQueue::countByStatusInternal() const {
    std::unordered_map<DeadLetterStatus, int> counts;
    for (const auto& pair : items_) {
        counts[pair.second.status]++;
    }
    return counts;
}

} // namespace WorkflowSystem
