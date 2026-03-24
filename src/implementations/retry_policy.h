/**
 * @file retry_policy.h
 * @brief 业务流程管理系统 - 重试策略实现
 *
 * 设计模式：
 * - 策略模式：实现不同的重试策略
 *
 * 面向对象：
 * - 封装：封装重试逻辑和延迟计算
 */

#ifndef WORKFLOW_SYSTEM_RETRY_POLICY_IMPL_H
#define WORKFLOW_SYSTEM_RETRY_POLICY_IMPL_H

#include "interfaces/retry_policy.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace WorkflowSystem {

/**
 * @brief 重试策略实现
 */
class RetryPolicy : public IRetryPolicy {
public:
    explicit RetryPolicy(const RetryConfig& config)
        : config_(config), currentAttempt_(0) {}

    bool shouldRetry(int attempt, const std::string& error, int64_t elapsedMs) override {
        // 检查是否超过最大重试次数
        if (attempt >= config_.maxRetries) {
            LOG_WARNING("[RetryPolicy] Max retries exceeded (" +
                       std::to_string(config_.maxRetries) + ")");
            return false;
        }

        // 检查重试条件
        bool conditionMet = false;

        switch (config_.condition) {
            case RetryCondition::ON_ERROR:
                conditionMet = !error.empty();
                break;

            case RetryCondition::ON_TIMEOUT:
                // 这里简单判断，实际应用中可能需要更复杂的逻辑
                conditionMet = elapsedMs > 0 && error.find("timeout") != std::string::npos;
                break;

            case RetryCondition::ON_EXCEPTION:
                conditionMet = !error.empty();
                break;

            case RetryCondition::ON_FAILURE:
                conditionMet = !error.empty() || elapsedMs > 0;
                break;

            case RetryCondition::CUSTOM:
                if (config_.customCondition) {
                    conditionMet = config_.customCondition(error, elapsedMs);
                }
                break;
        }

        if (!conditionMet) {
            return false;
        }

        currentAttempt_ = attempt;

        // 调用重试回调
        if (config_.onRetryCallback) {
            config_.onRetryCallback(attempt, error);
        }

        int64_t delay = getNextDelay(attempt);
        LOG_INFO("[RetryPolicy] Retrying (attempt " + std::to_string(attempt + 1) +
                  "/" + std::to_string(config_.maxRetries) +
                  ") after " + std::to_string(delay) + "ms delay");

        // 等待延迟
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        return true;
    }

    int64_t getNextDelay(int attempt) const override {
        int64_t delay = 0;

        switch (config_.strategy) {
            case RetryStrategy::FIXED_DELAY:
                delay = config_.initialDelayMs;
                break;

            case RetryStrategy::EXPONENTIAL_BACKOFF:
                // 指数退避：delay = initial * (multiplier ^ attempt)
                delay = static_cast<int64_t>(
                    config_.initialDelayMs * std::pow(config_.backoffMultiplier, attempt)
                );
                break;

            case RetryStrategy::LINEAR_BACKOFF:
                // 线性退避：delay = initial + increment * attempt
                delay = config_.initialDelayMs + config_.delayIncrementMs * attempt;
                break;

            case RetryStrategy::IMMEDIATE:
                delay = 0;
                break;
        }

        // 限制最大延迟
        return std::min(delay, config_.maxDelayMs);
    }

    void reset() override {
        currentAttempt_ = 0;
        LOG_INFO("[RetryPolicy] Retry counter reset");
    }

    int getCurrentAttempt() const override {
        return currentAttempt_;
    }

    int getMaxRetries() const override {
        return config_.maxRetries;
    }

    bool hasReachedMaxRetries() const override {
        return currentAttempt_ >= config_.maxRetries;
    }

    /**
     * @brief 更新配置
     */
    void setConfig(const RetryConfig& config) {
        config_ = config;
        currentAttempt_ = 0;
    }

    /**
     * @brief 获取当前配置
     */
    const RetryConfig& getConfig() const {
        return config_;
    }

private:
    RetryConfig config_;
    int currentAttempt_;
};

/**
 * @brief 重试策略工厂
 */
class RetryPolicyFactory {
public:
    // 创建固定延迟重试策略
    static std::shared_ptr<RetryPolicy> createFixedDelay(
        int maxRetries, int64_t delayMs) {
        RetryConfig config;
        config.maxRetries = maxRetries;
        config.strategy = RetryStrategy::FIXED_DELAY;
        config.initialDelayMs = delayMs;
        return std::make_shared<RetryPolicy>(config);
    }

    // 创建指数退避重试策略
    static std::shared_ptr<RetryPolicy> createExponentialBackoff(
        int maxRetries, int64_t initialDelayMs, double multiplier = 2.0,
        int64_t maxDelayMs = 30000) {
        RetryConfig config;
        config.maxRetries = maxRetries;
        config.strategy = RetryStrategy::EXPONENTIAL_BACKOFF;
        config.initialDelayMs = initialDelayMs;
        config.backoffMultiplier = multiplier;
        config.maxDelayMs = maxDelayMs;
        return std::make_shared<RetryPolicy>(config);
    }

    // 创建线性退避重试策略
    static std::shared_ptr<RetryPolicy> createLinearBackoff(
        int maxRetries, int64_t initialDelayMs, int64_t incrementMs,
        int64_t maxDelayMs = 30000) {
        RetryConfig config;
        config.maxRetries = maxRetries;
        config.strategy = RetryStrategy::LINEAR_BACKOFF;
        config.initialDelayMs = initialDelayMs;
        config.delayIncrementMs = incrementMs;
        config.maxDelayMs = maxDelayMs;
        return std::make_shared<RetryPolicy>(config);
    }

    // 创建立即重试策略
    static std::shared_ptr<RetryPolicy> createImmediate(int maxRetries) {
        RetryConfig config;
        config.maxRetries = maxRetries;
        config.strategy = RetryStrategy::IMMEDIATE;
        return std::make_shared<RetryPolicy>(config);
    }

    // 使用自定义配置创建
    static std::shared_ptr<RetryPolicy> createCustom(const RetryConfig& config) {
        return std::make_shared<RetryPolicy>(config);
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_RETRY_POLICY_IMPL_H
