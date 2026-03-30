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
#include "workflow_system/interfaces/dead_letter_queue.h"

namespace WorkflowSystem {

/**
 * @brief 死信队列实现
 */
class DeadLetterQueue : public IDeadLetterQueue {
public:
    DeadLetterQueue();
    ~DeadLetterQueue() override = default;

    // ========== 入队操作 ==========
    std::string enqueue(const DeadLetterItem& item) override;
    std::string enqueueFromError(const std::string& workflowName,
                                const std::string& errorMessage,
                                DeadLetterReason reason) override;

    // ========== 出队和查询 ==========
    DeadLetterItem getItem(const std::string& itemId) const override;
    std::vector<DeadLetterItem> getPendingItems() const override;
    std::vector<DeadLetterItem> getItemsByStatus(DeadLetterStatus status, int limit) const override;
    std::vector<DeadLetterItem> getItemsByWorkflow(const std::string& workflowName) const override;
    size_t size() const override;
    bool isEmpty() const override;

    // ========== 重试操作 ==========
    bool retry(const std::string& itemId) override;
    int retryBatch(const std::vector<std::string>& itemIds) override;
    int retryAll() override;

    // ========== 解决操作 ==========
    bool resolve(const std::string& itemId,
                const std::string& resolution,
                const std::string& resolvedBy) override;
    bool discard(const std::string& itemId, const std::string& reason) override;
    bool markForManualReview(const std::string& itemId) override;

    // ========== 管理操作 ==========
    int clearResolved() override;
    void clearAll() override;
    int clearExpired() override;
    void setConfig(const DeadLetterQueueConfig& config) override;
    DeadLetterQueueConfig getConfig() const override;

    // ========== 回调设置 ==========
    void setEnqueueCallback(DeadLetterCallback callback) override;
    void setRetryCallback(DeadLetterCallback callback) override;
    void setResolveCallback(DeadLetterCallback callback) override;

    // ========== 统计 ==========
    std::string getStatistics() const override;
    std::unordered_map<DeadLetterReason, int> countByReason() const override;
    std::unordered_map<DeadLetterStatus, int> countByStatus() const override;

private:
    mutable std::mutex mutex_;
    DeadLetterQueueConfig config_;

    std::unordered_map<std::string, DeadLetterItem> items_;
    std::deque<std::string> pendingQueue_;

    DeadLetterCallback enqueueCallback_;
    DeadLetterCallback retryCallback_;
    DeadLetterCallback resolveCallback_;

    void removeOldestItem();
    std::unordered_map<DeadLetterReason, int> countByReasonInternal() const;
    std::unordered_map<DeadLetterStatus, int> countByStatusInternal() const;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_IMPL_H
