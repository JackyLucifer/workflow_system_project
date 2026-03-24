/**
 * @file dead_letter_queue_impl.h
 * @brief 死信队列实现
 *
 * 功能：存储失败任务、重试处理、人工干预
 */

#ifndef WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_IMPL_H
#define WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_IMPL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <algorithm>
#include "interfaces/dead_letter_queue.h"
#include "core/logger.h"
#include "core/utils.h"

namespace WorkflowSystem {

/**
 * @brief 死信队列实现
 */
class DeadLetterQueue : public IDeadLetterQueue {
public:
    DeadLetterQueue() : config_() {}
    
    ~DeadLetterQueue() override = default;
    
    // ========== 入队操作 ==========
    
    std::string enqueue(const DeadLetterItem& item) override {
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
    
    std::string enqueueFromError(const std::string& workflowName,
                                const std::string& errorMessage,
                                DeadLetterReason reason) override {
        DeadLetterItem item;
        item.workflowName = workflowName;
        item.errorMessage = errorMessage;
        item.reason = reason;
        item.maxRetries = config_.maxRetries;
        
        return enqueue(item);
    }
    
    // ========== 出队和查询 ==========
    
    DeadLetterItem getItem(const std::string& itemId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = items_.find(itemId);
        if (it != items_.end()) {
            return it->second;
        }
        return DeadLetterItem();
    }
    
    std::vector<DeadLetterItem> getPendingItems() const override {
        return getItemsByStatus(DeadLetterStatus::PENDING, 1000);
    }
    
    std::vector<DeadLetterItem> getItemsByStatus(DeadLetterStatus status, int limit) const override {
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
    
    std::vector<DeadLetterItem> getItemsByWorkflow(const std::string& workflowName) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<DeadLetterItem> result;
        for (const auto& pair : items_) {
            if (pair.second.workflowName == workflowName) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }
    
    bool isEmpty() const override {
        return size() == 0;
    }
    
    // ========== 重试操作 ==========
    
    bool retry(const std::string& itemId) override {
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
    
    int retryBatch(const std::vector<std::string>& itemIds) override {
        int successCount = 0;
        for (const auto& itemId : itemIds) {
            if (retry(itemId)) {
                successCount++;
            }
        }
        return successCount;
    }
    
    int retryAll() override {
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
    
    bool resolve(const std::string& itemId,
                const std::string& resolution,
                const std::string& resolvedBy) override {
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
    
    bool discard(const std::string& itemId, const std::string& reason) override {
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
    
    bool markForManualReview(const std::string& itemId) override {
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
    
    int clearResolved() override {
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
    
    void clearAll() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        items_.clear();
        pendingQueue_.clear();
        
        LOG_INFO("[DeadLetterQueue] Cleared all items");
    }
    
    int clearExpired() override {
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
    
    void setConfig(const DeadLetterQueueConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }
    
    DeadLetterQueueConfig getConfig() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    // ========== 回调设置 ==========
    
    void setEnqueueCallback(DeadLetterCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        enqueueCallback_ = callback;
    }
    
    void setRetryCallback(DeadLetterCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        retryCallback_ = callback;
    }
    
    void setResolveCallback(DeadLetterCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        resolveCallback_ = callback;
    }
    
    // ========== 统计 ==========
    
    std::string getStatistics() const override {
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
    
    std::unordered_map<DeadLetterReason, int> countByReason() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return countByReasonInternal();
    }
    
    std::unordered_map<DeadLetterStatus, int> countByStatus() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return countByStatusInternal();
    }

private:
    mutable std::mutex mutex_;
    DeadLetterQueueConfig config_;
    
    std::unordered_map<std::string, DeadLetterItem> items_;
    std::deque<std::string> pendingQueue_;
    
    DeadLetterCallback enqueueCallback_;
    DeadLetterCallback retryCallback_;
    DeadLetterCallback resolveCallback_;
    
    void removeOldestItem() {
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
    
    std::unordered_map<DeadLetterReason, int> countByReasonInternal() const {
        std::unordered_map<DeadLetterReason, int> counts;
        for (const auto& pair : items_) {
            counts[pair.second.reason]++;
        }
        return counts;
    }
    
    std::unordered_map<DeadLetterStatus, int> countByStatusInternal() const {
        std::unordered_map<DeadLetterStatus, int> counts;
        for (const auto& pair : items_) {
            counts[pair.second.status]++;
        }
        return counts;
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_IMPL_H
