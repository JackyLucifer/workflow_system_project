/**
 * @file dead_letter_queue.h
 * @brief 死信队列实现
 */

#ifndef WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_H
#define WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_H

#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>

namespace WorkflowSystem {

struct FailedTask {
    std::string taskId;
    std::string nodeId;
    std::string error;
    std::chrono::system_clock::time_point timestamp;
    int retryCount;
    bool shouldRetry;
};

class DeadLetterQueue {
public:
    DeadLetterQueue() : taskCounter_(0) {}

    void enqueue(const std::string& nodeId, const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);

        FailedTask task;
        task.taskId = "task_" + std::to_string(taskCounter_++);
        task.nodeId = nodeId;
        task.error = error;
        task.timestamp = std::chrono::system_clock::now();
        task.retryCount = 0;
        task.shouldRetry = true;

        queue_.push(task);
        taskMap_[task.taskId] = task;
    }

    bool dequeue(int count, std::vector<FailedTask>& tasks) {
        std::lock_guard<std::mutex> lock(mutex_);

        int retrieved = 0;
        while (!queue_.empty() && retrieved < count) {
            tasks.push_back(queue_.front());
            queue_.pop();
            ++retrieved;
        }

        return retrieved > 0;
    }

    bool retry(const std::string& taskId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = taskMap_.find(taskId);
        if (it == taskMap_.end()) {
            return false;
        }

        it->second.retryCount++;
        queue_.push(it->second);
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        taskMap_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::queue<FailedTask> queue_;
    std::unordered_map<std::string, FailedTask> taskMap_;
    std::atomic<uint64_t> taskCounter_{0};
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_DEAD_LETTER_QUEUE_H
