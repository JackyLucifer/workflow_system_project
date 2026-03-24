/**
 * @file logger.h
 * @brief 业务流程管理系统 - 日志模块
 *
 * 设计模式：
 * - 单例模式：ConsoleLogger 确保全局唯一日志实例
 * - 接口与实现分离：ILogger 接口，ConsoleLogger 实现
 *
 * 面向对象：
 * - 封装：隐藏日志实现细节
 * - 多态：通过 ILogger 接口支持不同日志实现
 */

#ifndef WORKFLOW_SYSTEM_LOGGER_H
#define WORKFLOW_SYSTEM_LOGGER_H

#include <iostream>
#include <mutex>
#include <memory>

namespace WorkflowSystem {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief 日志接口
 *
 * 接口与实现分离：定义日志接口，便于扩展不同的日志实现
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void info(const std::string& message) = 0;
    virtual void warning(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
};

/**
 * @brief 控制台日志实现
 *
 * 设计模式：单例模式
 * 确保整个应用程序只有一个日志实例
 */
class ConsoleLogger : public ILogger {
public:
    // 单例访问点
    static ConsoleLogger& getInstance() {
        static ConsoleLogger instance;
        return instance;
    }

    // 删除拷贝构造和赋值（单例模式必需）
    ConsoleLogger(const ConsoleLogger&) = delete;
    ConsoleLogger& operator=(const ConsoleLogger&) = delete;

    void setLevel(LogLevel level) override {
        std::lock_guard<std::mutex> lock(mutex_);
        level_ = level;
    }

    LogLevel getLevel() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return level_;
    }

    void info(const std::string& message) override {
        if (level_ <= LogLevel::INFO) {
            log("[INFO]", message);
        }
    }

    void warning(const std::string& message) override {
        if (level_ <= LogLevel::WARNING) {
            log("[WARNING]", message);
        }
    }

    void error(const std::string& message) override {
        if (level_ <= LogLevel::ERROR) {
            log("[ERROR]", message);
        }
    }

private:
    ConsoleLogger() : level_(LogLevel::INFO) {}  // 私有构造函数

    void log(const std::string& level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << level << " " << message << std::endl;
    }

    mutable std::mutex mutex_;
    LogLevel level_;
};

/**
 * @brief 简化的 Logger 类（兼容旧代码）
 */
class Logger : public ILogger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void setLevel(LogLevel level) override {
        level_ = level;
    }

    LogLevel getLevel() const override {
        return level_;
    }

    void info(const std::string& message) override {
        if (level_ <= LogLevel::INFO) {
            ConsoleLogger::getInstance().info(message);
        }
    }

    void warning(const std::string& message) override {
        if (level_ <= LogLevel::WARNING) {
            ConsoleLogger::getInstance().warning(message);
        }
    }

    void error(const std::string& message) override {
        if (level_ <= LogLevel::ERROR) {
            ConsoleLogger::getInstance().error(message);
        }
    }

private:
    Logger() : level_(LogLevel::INFO) {}
    LogLevel level_;
};

// 日志宏 - 便捷访问日志
#define LOG_INFO(msg) WorkflowSystem::ConsoleLogger::getInstance().info(msg)
#define LOG_WARNING(msg) WorkflowSystem::ConsoleLogger::getInstance().warning(msg)
#define LOG_ERROR(msg) WorkflowSystem::ConsoleLogger::getInstance().error(msg)

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_LOGGER_H
