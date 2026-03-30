#ifndef PLUGIN_FRAMEWORK_LOGGER_HPP
#define PLUGIN_FRAMEWORK_LOGGER_HPP

#include <string>
#include <sstream>
#include <mutex>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <iomanip>
#include <functional>
#include <thread>
#include <map>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 日志级别
 */
enum class LogLevel {
    TRACE = 0,      // 最详细的追踪信息
    DEBUG = 1,       // 调试信息
    INFO = 2,        // 一般信息
    WARNING = 3,     // 警告信息
    ERROR = 4,       // 错误信息
    FATAL = 5        // 致命错误
};

/**
 * @brief 日志级别转字符串
 */
inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:               return "UNKNOWN";
    }
}

/**
 * @brief 日志记录
 */
struct LogRecord {
    LogLevel level;
    std::string message;
    std::string loggerName;
    std::string sourceFile;
    int sourceLine;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id threadId;

    std::string toString() const {
        std::ostringstream oss;
        auto time = std::chrono::system_clock::to_time_t(timestamp);
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
            << "." << std::setw(3) << std::setfill('0')
            << (std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count() % 1000)
            << " [" << logLevelToString(level) << "]"
            << " [" << loggerName << "]";

        if (!sourceFile.empty()) {
            oss << " [" << sourceFile << ":" << sourceLine << "]";
        }

        oss << " " << message;
        return oss.str();
    }
};

/**
 * @brief 日志输出器接口
 */
class ILogAppender {
public:
    virtual ~ILogAppender() = default;

    /**
     * @brief 写入日志记录
     */
    virtual void append(const LogRecord& record) = 0;

    /**
     * @brief 刷新缓冲区
     */
    virtual void flush() = 0;
};

/**
 * @brief 控制台输出器
 */
class ConsoleAppender : public ILogAppender {
public:
    void append(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // 设置颜色
        std::string colorStart;
        std::string colorEnd = "\033[0m";

        switch (record.level) {
            case LogLevel::TRACE:
            case LogLevel::DEBUG:
                colorStart = "\033[36m";  // 青色
                break;
            case LogLevel::INFO:
                colorStart = "\033[32m";  // 绿色
                break;
            case LogLevel::WARNING:
                colorStart = "\033[33m";  // 黄色
                break;
            case LogLevel::ERROR:
            case LogLevel::FATAL:
                colorStart = "\033[31m";  // 红色
                break;
            default:
                colorStart = "\033[0m";
                colorEnd = "\033[0m";
        }

        std::cout << colorStart << record.toString() << colorEnd << std::endl;
    }

    void flush() override {
        std::cout.flush();
    }

private:
    std::mutex mutex_;
};

/**
 * @brief 文件输出器
 */
class FileAppender : public ILogAppender {
public:
    explicit FileAppender(const std::string& filename,
                          size_t maxSize = 10 * 1024 * 1024)  // 10MB
        : filename_(filename)
        , maxSize_(maxSize)
        , currentSize_(0) {
        openFile();
    }

    ~FileAppender() override {
        closeFile();
    }

    void append(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!file_.is_open()) {
            return;
        }

        std::string message = record.toString() + "\n";
        file_ << message;
        file_.flush();

        currentSize_ += message.length();

        // 检查是否需要轮转
        if (currentSize_ >= maxSize_) {
            rotateFile();
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }

private:
    void openFile() {
        file_.open(filename_, std::ios::out | std::ios::app);
        if (file_.is_open()) {
            file_.seekp(0, std::ios::end);
            currentSize_ = file_.tellp();
        }
    }

    void closeFile() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    void rotateFile() {
        closeFile();

        // 重命名现有文件
        int index = 1;
        std::string backupName;
        do {
            std::ostringstream oss;
            oss << filename_ << "." << index++;
            backupName = oss.str();
        } while (std::ifstream(backupName).good());

        std::rename(filename_.c_str(), backupName.c_str());
        openFile();
    }

    std::string filename_;
    size_t maxSize_;
    size_t currentSize_;
    std::ofstream file_;
    std::mutex mutex_;
};

/**
 * @brief 日志过滤器
 */
using LogFilter = std::function<bool(const LogRecord&)>;

/**
 * @brief 日志器
 */
class Logger {
public:
    static Logger& getLogger(const std::string& name = "root") {
        static std::map<std::string, std::unique_ptr<Logger>> loggers;
        static std::mutex mutex;

        std::lock_guard<std::mutex> lock(mutex);
        auto it = loggers.find(name);
        if (it == loggers.end()) {
            auto logger = std::unique_ptr<Logger>(new Logger(name));
            it = loggers.emplace(name, std::move(logger)).first;
        }
        return *it->second;
    }

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel level) {
        level_ = level;
    }

    /**
     * @brief 获取日志级别
     */
    LogLevel getLevel() const {
        return level_;
    }

    /**
     * @brief 添加输出器
     */
    void addAppender(std::shared_ptr<ILogAppender> appender) {
        std::lock_guard<std::mutex> lock(mutex_);
        appenders_.push_back(appender);
    }

    /**
     * @brief 清除所有输出器
     */
    void clearAppenders() {
        std::lock_guard<std::mutex> lock(mutex_);
        appenders_.clear();
    }

    /**
     * @brief 添加过滤器
     */
    void addFilter(LogFilter filter) {
        std::lock_guard<std::mutex> lock(mutex_);
        filters_.push_back(filter);
    }

    /**
     * @brief 清除所有过滤器
     */
    void clearFilters() {
        std::lock_guard<std::mutex> lock(mutex_);
        filters_.clear();
    }

    /**
     * @brief 记录日志
     */
    void log(LogLevel level, const std::string& message,
            const std::string& file = "", int line = 0) {
        if (level < level_) {
            return;
        }

        LogRecord record;
        record.level = level;
        record.message = message;
        record.loggerName = name_;
        record.sourceFile = file;
        record.sourceLine = line;
        record.timestamp = std::chrono::system_clock::now();
        record.threadId = std::this_thread::get_id();

        // 应用过滤器
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& filter : filters_) {
                if (!filter(record)) {
                    return;  // 被过滤掉
                }
            }

            for (auto& appender : appenders_) {
                appender->append(record);
            }
        }
    }

    /**
     * @brief 刷新所有输出器
     */
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& appender : appenders_) {
            appender->flush();
        }
    }

    // 便捷方法
    void trace(const std::string& msg, const std::string& file = "", int line = 0) {
        log(LogLevel::TRACE, msg, file, line);
    }

    void debug(const std::string& msg, const std::string& file = "", int line = 0) {
        log(LogLevel::DEBUG, msg, file, line);
    }

    void info(const std::string& msg, const std::string& file = "", int line = 0) {
        log(LogLevel::INFO, msg, file, line);
    }

    void warning(const std::string& msg, const std::string& file = "", int line = 0) {
        log(LogLevel::WARNING, msg, file, line);
    }

    void error(const std::string& msg, const std::string& file = "", int line = 0) {
        log(LogLevel::ERROR, msg, file, line);
    }

    void fatal(const std::string& msg, const std::string& file = "", int line = 0) {
        log(LogLevel::FATAL, msg, file, line);
    }

private:
    explicit Logger(const std::string& name)
        : name_(name), level_(LogLevel::INFO) {
        // 默认添加控制台输出器
        addAppender(std::make_shared<ConsoleAppender>());
    }

    std::string name_;
    LogLevel level_;
    std::vector<std::shared_ptr<ILogAppender>> appenders_;
    std::vector<LogFilter> filters_;
    std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

// 便捷宏
#define PF_LOG_TRACE(logger, msg) \
    (logger).trace((msg), __FILE__, __LINE__)

#define PF_LOG_DEBUG(logger, msg) \
    (logger).debug((msg), __FILE__, __LINE__)

#define PF_LOG_INFO(logger, msg) \
    (logger).info((msg), __FILE__, __LINE__)

#define PF_LOG_WARNING(logger, msg) \
    (logger).warning((msg), __FILE__, __LINE__)

#define PF_LOG_ERROR(logger, msg) \
    (logger).error((msg), __FILE__, __LINE__)

#define PF_LOG_FATAL(logger, msg) \
    (logger).fatal((msg), __FILE__, __LINE__)

// 获取根日志器
#define PF_GET_LOGGER() \
    WorkflowSystem::Plugin::Logger::getLogger("root")

// 简化的日志宏
#define PF_TRACE(msg) PF_LOG_TRACE(PF_GET_LOGGER(), msg)
#define PF_DEBUG(msg) PF_LOG_DEBUG(PF_GET_LOGGER(), msg)
#define PF_INFO(msg)  PF_LOG_INFO(PF_GET_LOGGER(), msg)
#define PF_WARN(msg)  PF_LOG_WARNING(PF_GET_LOGGER(), msg)
#define PF_ERROR(msg) PF_LOG_ERROR(PF_GET_LOGGER(), msg)
#define PF_FATAL(msg) PF_LOG_FATAL(PF_GET_LOGGER(), msg)

#endif // PLUGIN_FRAMEWORK_LOGGER_HPP
