/**
 * @file async_logger.cpp
 * @brief 异步日志系统实现
 */

#include "workflow_system/implementations/async_logger.h"
#include "workflow_system/core/logger.h"
#include <ctime>
#include <iostream>

namespace WorkflowSystem {

// ========== AsyncLogger 私有构造函数 ==========

AsyncLogger::AsyncLogger() : logFile_("workflow.log"), running_(true) {
    // 检查文件是否成功打开
    if (!logFile_.isOpen() && config_.errorCallback) {
        config_.errorCallback("Failed to open log file: " + config_.logFilePath);
    }

    // 启动后台线程
    workerThread_ = std::thread([this]() {
        while (running_) {
            LogEntry entry;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueNotEmpty_.wait(lock, [this]() {
                    return !logQueue_.empty() || !running_;
                });

                if (!running_) break;
                if (logQueue_.empty()) continue;

                entry = logQueue_.front();
                logQueue_.pop();
            }

            // 格式化日志
            std::string formattedLog = formatLog(entry);

            // 写入控制台
            if (config_.enableConsole) {
                std::cout << formattedLog << std::endl;
            }

            // 写入文件（RAII管理）
            if (config_.enableFile && logFile_.isOpen()) {
                logFile_.write(formattedLog);
                if (config_.flushOnWrite) {
                    logFile_.flush();
                }
            }
        }
    });
}

AsyncLogger::~AsyncLogger() {
    shutdown();
}

// ========== LogFile 类实现 ==========

void LogFile::open() {
    try {
        file_.open(path_, std::ios::app);
        isOpen_ = file_.is_open();
    } catch (const std::exception& e) {
        isOpen_ = false;
        // 错误将通过config_.errorCallback报告
    }
}

void LogFile::close() {
    if (file_.is_open()) {
        file_.close();
    }
    isOpen_ = false;
}

void LogFile::write(const std::string& message) {
    if (isOpen_ && file_.is_open()) {
        file_ << message << std::endl;
    }
}

void LogFile::flush() {
    if (isOpen_ && file_.is_open()) {
        file_.flush();
    }
}

void LogFile::reopen() {
    close();
    open();
}

// ========== AsyncLogger 类实现 ==========

void AsyncLogger::setConfig(const AsyncLoggerConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

AsyncLoggerConfig AsyncLogger::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void AsyncLogger::log(const std::string& message, LogLevel level) {
    LogEntry entry;
    entry.message = message;
    entry.level = level;
    entry.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);

        // 检查队列长度限制
        if (config_.maxQueueSize > 0 &&
            logQueue_.size() >= config_.maxQueueSize) {
            // 队列已满，丢弃最旧的日志
            logQueue_.pop();
        }

        logQueue_.push(entry);
    }
    queueNotEmpty_.notify_one();
}

void AsyncLogger::info(const std::string& msg) {
    log(msg, LogLevel::INFO);
}

void AsyncLogger::warning(const std::string& msg) {
    log(msg, LogLevel::WARNING);
}

void AsyncLogger::error(const std::string& msg) {
    log(msg, LogLevel::ERROR);
}

void AsyncLogger::debug(const std::string& msg) {
    log(msg, LogLevel::INFO);  // 暂时使用 INFO 级别
}

void AsyncLogger::shutdown() {
    running_ = false;
    queueNotEmpty_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    // LogFile 的析构函数会自动关闭文件
}

std::string AsyncLogger::formatLog(const LogEntry& entry) {
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::string tm = std::ctime(&time_t);
    if (!tm.empty()) {
        tm.pop_back(); // 移除换行符
    }

    std::ostringstream oss;
    oss << "[" << tm << "]";

    switch (entry.level) {
        case LogLevel::INFO: oss << "[INFO]"; break;
        case LogLevel::WARNING: oss << "[WARNING]"; break;
        case LogLevel::ERROR: oss << "[ERROR]"; break;
    }

    oss << " " << entry.message;
    return oss.str();
}

// AsyncLogger 的私有构造函数和析构函数已经在头文件中实现
// 因为它们是私有的，并且需要初始化成员变量

} // namespace WorkflowSystem
