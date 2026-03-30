/**
 * @file circuit_breaker.h
 * @brief 熔断器实现 - 弹性模式
 */

#ifndef WORKFLOW_SYSTEM_CIRCUIT_BREAKER_H
#define WORKFLOW_SYSTEM_CIRCUIT_BREAKER_H

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include "workflow_system/core/types.h"

namespace WorkflowSystem {

/**
 * @brief 熔断器状态
 */
enum class CircuitState {
    CLOSED,         // 关闭状态（正常，允许请求）
    OPEN,           // 开启状态（熔断，拒绝请求）
    HALF_OPEN       // 半开状态（尝试恢复）
};

/**
 * @brief 熔断器配置
 */
struct CircuitBreakerConfig {
    int failureThreshold;           // 失败阈值
    int64_t timeoutMs;              // 超时时间（毫秒）
    int64_t resetTimeoutMs;         // 重置超时（毫秒）
    int halfOpenThreshold;          // 半开状态下成功阈值
    bool enabled;                   // 是否启用
    
    CircuitBreakerConfig()
        : failureThreshold(5)
        , timeoutMs(10000)
        , resetTimeoutMs(30000)
        , halfOpenThreshold(3)
        , enabled(true) {}
};

/**
 * @brief 熔断器统计
 */
struct CircuitBreakerStats {
    CircuitState state;
    int failureCount;               // 失败次数
    int successCount;               // 成功次数
    int64_t lastFailureTime;        // 最后失败时间
    int64_t lastSuccessTime;        // 最后成功时间
    int64_t totalOpenTime;          // 总开启时间
    
    CircuitBreakerStats()
        : state(CircuitState::CLOSED)
        , failureCount(0)
        , successCount(0)
        , lastFailureTime(0)
        , lastSuccessTime(0)
        , totalOpenTime(0) {}
};

/**
 * @brief 熔断器接口
 */
class ICircuitBreaker {
public:
    virtual ~ICircuitBreaker() = default;
    
    virtual CircuitState getState() const = 0;
    virtual bool allowRequest() const = 0;
    virtual void recordSuccess() = 0;
    virtual void recordFailure() = 0;
    virtual CircuitBreakerStats getStats() const = 0;
    virtual void reset() = 0;
    virtual void forceOpen() = 0;
    virtual void forceClose() = 0;
    virtual void setConfig(const CircuitBreakerConfig& config) = 0;
    virtual CircuitBreakerConfig getConfig() const = 0;
    
    using StateChangeCallback = std::function<void(CircuitState oldState, CircuitState newState)>;
    virtual void setStateChangeCallback(StateChangeCallback callback) = 0;
};

/**
 * @brief 熔断器实现
 */
class CircuitBreaker : public ICircuitBreaker {
public:
    CircuitBreaker() 
        : config_()
        , state_(CircuitState::CLOSED)
        , failureCount_(0)
        , successCount_(0)
        , lastFailureTime_(0)
        , lastSuccessTime_(0)
        , openTime_(0)
        , lastStateChangeTime_(0)
        , stateChangeCallback_() {}
    
    explicit CircuitBreaker(const CircuitBreakerConfig& config)
        : config_(config)
        , state_(CircuitState::CLOSED)
        , failureCount_(0)
        , successCount_(0)
        , lastFailureTime_(0)
        , lastSuccessTime_(0)
        , openTime_(0)
        , lastStateChangeTime_(0)
        , stateChangeCallback_() {}
    
    CircuitState getState() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 检查是否应该从 OPEN 转为 HALF_OPEN
        if (state_ == CircuitState::OPEN && openTime_ > 0) {
            int64_t elapsed = getCurrentMs() - openTime_;
            if (elapsed >= config_.resetTimeoutMs) {
                return CircuitState::HALF_OPEN;
            }
        }
        return state_;
    }
    
    bool allowRequest() const override {
        if (!config_.enabled) {
            return true;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        switch (state_) {
            case CircuitState::CLOSED:
                return true;
            
            case CircuitState::OPEN: {
                int64_t elapsed = getCurrentMs() - openTime_;
                if (elapsed >= config_.resetTimeoutMs) {
                    return true;
                }
                return false;
            }
            
            case CircuitState::HALF_OPEN:
                return true;
            
            default:
                return false;
        }
    }
    
    void recordSuccess() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        successCount_++;
        lastSuccessTime_ = getCurrentMs();
        
        if (state_ == CircuitState::HALF_OPEN) {
            int halfOpenSuccessCount = 0;
            if (successCount_ >= config_.halfOpenThreshold) {
                halfOpenSuccessCount = successCount_;
            }
            if (halfOpenSuccessCount >= config_.halfOpenThreshold) {
                setState(CircuitState::CLOSED);
                failureCount_ = 0;
                successCount_ = 0;
            }
        } else if (state_ == CircuitState::OPEN) {
            int64_t elapsed = getCurrentMs() - openTime_;
            if (elapsed >= config_.resetTimeoutMs) {
                setState(CircuitState::HALF_OPEN);
            }
        }
    }
    
    void recordFailure() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        failureCount_++;
        lastFailureTime_ = getCurrentMs();
        
        if (state_ == CircuitState::CLOSED) {
            if (failureCount_ >= config_.failureThreshold) {
                setState(CircuitState::OPEN);
                openTime_ = getCurrentMs();
            }
        } else if (state_ == CircuitState::HALF_OPEN) {
            setState(CircuitState::OPEN);
            openTime_ = getCurrentMs();
        } else if (state_ == CircuitState::OPEN) {
            openTime_ = getCurrentMs();
        }
    }
    
    CircuitBreakerStats getStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        CircuitBreakerStats stats;
        stats.state = state_;
        stats.failureCount = failureCount_;
        stats.successCount = successCount_;
        stats.lastFailureTime = lastFailureTime_;
        stats.lastSuccessTime = lastSuccessTime_;
        
        if (state_ == CircuitState::OPEN && openTime_ > 0) {
            stats.totalOpenTime = getCurrentMs() - openTime_;
        }
        
        return stats;
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        setState(CircuitState::CLOSED);
        failureCount_ = 0;
        successCount_ = 0;
        openTime_ = 0;
    }
    
    void forceOpen() override {
        std::lock_guard<std::mutex> lock(mutex_);
        setState(CircuitState::OPEN);
        openTime_ = getCurrentMs();
    }
    
    void forceClose() override {
        std::lock_guard<std::mutex> lock(mutex_);
        setState(CircuitState::CLOSED);
        failureCount_ = 0;
        successCount_ = 0;
        openTime_ = 0;
    }
    
    void setConfig(const CircuitBreakerConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }
    
    CircuitBreakerConfig getConfig() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    void setStateChangeCallback(StateChangeCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        stateChangeCallback_ = callback;
    }

private:
    mutable std::mutex mutex_;
    CircuitBreakerConfig config_;
    CircuitState state_;
    int failureCount_;
    int successCount_;
    int64_t lastFailureTime_;
    int64_t lastSuccessTime_;
    int64_t openTime_;
    int64_t lastStateChangeTime_;
    StateChangeCallback stateChangeCallback_;
    
    void setState(CircuitState newState) {
        CircuitState oldState = state_;
        state_ = newState;
        lastStateChangeTime_ = getCurrentMs();
        
        if (stateChangeCallback_ && oldState != newState) {
            StateChangeCallback callback = stateChangeCallback_;
            mutex_.unlock();
            callback(oldState, newState);
            mutex_.lock();
        }
    }
    
    static int64_t getCurrentMs() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

inline std::string circuitStateToString(CircuitState state) {
    switch (state) {
        case CircuitState::CLOSED: return "closed";
        case CircuitState::OPEN: return "open";
        case CircuitState::HALF_OPEN: return "half_open";
        default: return "unknown";
    }
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CIRCUIT_BREAKER_H
