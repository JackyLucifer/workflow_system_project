/**
 * @file exceptions.h
 * @brief 统一的异常处理框架
 *
 * 功能：
 * - 定义统一的异常基类
 * - 提供具体的异常类型
 * - 统一异常消息格式
 * - 支持异常链
 * - 提供异常捕获和处理工具
 *
 * 使用示例：
 *   throw WorkflowException("Failed to execute workflow");
 *   throw WorkflowStateException("Invalid state transition", RUNNING, COMPLETED);
 */

#ifndef WORKFLOW_SYSTEM_EXCEPTIONS_H
#define WORKFLOW_SYSTEM_EXCEPTIONS_H

#include <stdexcept>
#include <string>
#include <sstream>
#include <memory>
#include <unordered_map>
#include "types.h"  // WorkflowState, stateToString
#include "utils.h"  // TimeUtils

namespace WorkflowSystem {

// ========== 异常基类 ==========

/**
 * @brief 工作流系统异常基类
 *
 * 所有自定义异常都应继承此类
 */
class WorkflowException : public std::runtime_error {
public:
    explicit WorkflowException(const std::string& message)
        : std::runtime_error(message)
        , context_()
        , cause_(nullptr) {}

    WorkflowException(const std::string& message, const std::exception& cause)
        : std::runtime_error(message)
        , context_()
        , cause_(std::current_exception()) {
        // 捕获异常链
    }

    virtual ~WorkflowException() noexcept = default;

    /**
     * @brief 添加上下文信息
     */
    void addContext(const std::string& key, const std::string& value) {
        context_[key] = value;
    }

    /**
     * @brief 获取完整消息（包含上下文）
     */
    virtual std::string getFullMessage() const {
        std::ostringstream oss;
        oss << what();

        if (!context_.empty()) {
            oss << " [Context: ";
            bool first = true;
            for (const auto& pair : context_) {
                if (!first) oss << ", ";
                oss << pair.first << "=" << pair.second;
                first = false;
            }
            oss << "]";
        }

        return oss.str();
    }

    /**
     * @brief 获取异常原因（如果有）
     */
    std::exception_ptr getCause() const { return cause_; }

protected:
    std::unordered_map<std::string, std::string> context_;
    std::exception_ptr cause_;
};

// ========== 具体异常类型 ==========

/**
 * @brief 状态异常
 *
 * 当工作流状态转换不合法时抛出
 */
class WorkflowStateException : public WorkflowException {
public:
    WorkflowStateException(const std::string& message,
                          WorkflowState from,
                          WorkflowState to)
        : WorkflowException(message)
        , fromState_(from)
        , toState_(to) {
        std::ostringstream oss;
        oss << message << " (from " << workflowStateToString(from)
           << " to " << workflowStateToString(to) << ")";
        message_ = oss.str();
    }

    const char* what() const noexcept override {
        return message_.c_str();
    }

    WorkflowState getFromState() const { return fromState_; }
    WorkflowState getToState() const { return toState_; }

private:
    WorkflowState fromState_;
    WorkflowState toState_;
    std::string message_;
};

/**
 * @brief 超时异常
 */
class TimeoutException : public WorkflowException {
public:
    explicit TimeoutException(const std::string& message, long timeoutMs)
        : WorkflowException(message)
        , timeoutMs_(timeoutMs) {
        std::ostringstream oss;
        oss << message << " (timeout: " << timeoutMs << "ms)";
        message_ = oss.str();
    }

    const char* what() const noexcept override {
        return message_.c_str();
    }

    long getTimeoutMs() const { return timeoutMs_; }

private:
    long timeoutMs_;
    std::string message_;
};

/**
 * @brief 验证异常
 *
 * 当输入验证失败时抛出
 */
class ValidationException : public WorkflowException {
public:
    explicit ValidationException(const std::string& field,
                                 const std::string& reason)
        : WorkflowException("Validation failed") {
        std::ostringstream oss;
        oss << "Field '" << field << "': " << reason;
        message_ = oss.str();
    }

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

/**
 * @brief 资源异常
 *
 * 当资源不足或不可用时抛出
 */
class ResourceException : public WorkflowException {
public:
    explicit ResourceException(const std::string& resourceType,
                               const std::string& resourceId)
        : WorkflowException("Resource unavailable") {
        std::ostringstream oss;
        oss << resourceType << " '" << resourceId << "' is unavailable";
        message_ = oss.str();
    }

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

/**
 * @brief 配置异常
 */
class ConfigurationException : public WorkflowException {
public:
    explicit ConfigurationException(const std::string& configKey,
                                    const std::string& reason)
        : WorkflowException("Configuration error") {
        std::ostringstream oss;
        oss << "Config key '" << configKey << "': " << reason;
        message_ = oss.str();
    }

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

// ========== 异常处理工具 ==========

/**
 * @brief 异常处理器
 *
 * 提供统一的异常捕获和处理逻辑
 */
class ExceptionHandler {
public:
    /**
     * @brief 处理异常并记录日志
     */
    static void handle(const std::exception& e,
                      const std::string& context = "") {
        std::ostringstream oss;
        if (!context.empty()) {
            oss << "[" << context << "] ";
        }
        oss << "Exception: " << e.what();

        // 记录错误日志
        LOG_ERROR_MSG(oss.str());
    }

    /**
     * @brief 处理未知异常
     */
    static void handleUnknown(const std::string& context = "") {
        std::ostringstream oss;
        if (!context.empty()) {
            oss << "[" << context << "] ";
        }
        oss << "Unknown exception";

        LOG_ERROR_MSG(oss.str());
    }

    /**
     * @brief 尝试执行，失败时处理异常
     */
    template<typename Func>
    static auto tryExecute(Func&& func,
                          const std::string& context = "",
                          bool rethrow = false) -> decltype(func()) {
        try {
            return func();
        } catch (const WorkflowException& e) {
            handle(e, context);
            if (rethrow) throw;
        } catch (const std::exception& e) {
            handle(e, context);
            if (rethrow) throw;
        } catch (...) {
            handleUnknown(context);
            if (rethrow) throw;
        }

        // 返回默认值（仅适用于有默认构造的类型）
        return decltype(func())();
    }
};

// ========== 异常安全宏 ==========

/**
 * @brief 异常安全执行宏
 *
 * 使用示例：
 *   TRY_WF {
 *       workflow->start();
 *   } CATCH_WF("Workflow execution") {
 *       // 异常已被处理
 *   }
 */
#define TRY_WF \
    try

#define CATCH_WF(context) \
    catch (const WorkflowSystem::WorkflowException& e) { \
        WorkflowSystem::ExceptionHandler::handle(e, context); \
    } catch (const std::exception& e) { \
        WorkflowSystem::ExceptionHandler::handle(e, context); \
    } catch (...) { \
        WorkflowSystem::ExceptionHandler::handleUnknown(context); \
    }

/**
 * @brief 必须成功执行宏
 *
 * 如果执行失败，记录错误并重新抛出异常
 */
#define TRY_WF_OR_THROW \
    try

#define CATCH_WF_OR_THROW(context) \
    catch (const WorkflowSystem::WorkflowException& e) { \
        WorkflowSystem::ExceptionHandler::handle(e, context); \
        throw; \
    } catch (const std::exception& e) { \
        WorkflowSystem::ExceptionHandler::handle(e, context); \
        throw; \
    } catch (...) { \
        WorkflowSystem::ExceptionHandler::handleUnknown(context); \
        throw std::runtime_error(std::string("Unknown exception in ") + context); \
    }

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_EXCEPTIONS_H
