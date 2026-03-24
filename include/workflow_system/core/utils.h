/**
 * @file utils.h
 * @brief 业务流程管理系统 - 工具类模块
 *
 * 面向对象：
 * - 工具类提供静态方法，无需实例化
 * - 每个工具类负责单一职责
 */

#ifndef WORKFLOW_SYSTEM_UTILS_H
#define WORKFLOW_SYSTEM_UTILS_H

#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace WorkflowSystem {

/**
 * @brief ID生成器
 *
 * 职责：生成唯一的资源ID
 */
class IdGenerator {
public:
    static std::string generate() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
        return "id_" + std::to_string(timestamp) + "_" + std::to_string(getCounter()++);
    }

private:
    // C++11兼容：使用函数返回静态变量引用
    static uint64_t& getCounter() {
        static uint64_t counter = 0;
        return counter;
    }
};

/**
 * @brief 时间工具类
 *
 * 职责：提供时间相关的辅助方法
 */
class TimeUtils {
public:
    /**
     * @brief 获取当前时间戳（格式：YYYYMMDD_HHMMSS）
     */
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    /**
     * @brief 获取当前毫秒时间戳
     */
    static int64_t getCurrentMs() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    /**
     * @brief 格式化持续时间（毫秒转为可读格式）
     * @param durationMs 持续时间（毫秒）
     * @return 格式化后的字符串，如 "2h 30m 15s 123ms"
     */
    static std::string formatDuration(int64_t durationMs) {
        std::stringstream ss;
        
        int64_t ms = durationMs % 1000;
        int64_t totalSeconds = durationMs / 1000;
        int64_t seconds = totalSeconds % 60;
        int64_t totalMinutes = totalSeconds / 60;
        int64_t minutes = totalMinutes % 60;
        int64_t hours = totalMinutes / 60;
        int64_t days = hours / 24;
        hours = hours % 24;

        bool hasContent = false;
        
        if (days > 0) {
            ss << days << "d ";
            hasContent = true;
        }
        if (hours > 0 || hasContent) {
            ss << hours << "h ";
            hasContent = true;
        }
        if (minutes > 0 || hasContent) {
            ss << minutes << "m ";
            hasContent = true;
        }
        if (seconds > 0 || hasContent) {
            ss << seconds << "s ";
        }
        ss << ms << "ms";
        
        return ss.str();
    }

    /**
     * @brief 格式化时间为 ISO 8601 格式
     * @return 格式化后的字符串，如 "2024-03-16T10:30:45"
     */
    static std::string formatIso8601() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
        return ss.str();
    }

    /**
     * @brief 计算两个时间戳之间的毫秒差
     */
    static int64_t diffMs(int64_t startMs, int64_t endMs) {
        return endMs - startMs;
    }
};

/**
 * @brief 字符串工具类
 *
 * 职责：提供字符串处理的辅助方法
 */
class StringUtils {
public:
    /**
     * @brief 检查字符串是否包含子串
     */
    static bool contains(const std::string& str, const std::string& substr) {
        return str.find(substr) != std::string::npos;
    }

    /**
     * @brief 连接路径
     */
    static std::string joinPath(const std::string& base, const std::string& relative) {
        if (base.empty()) return relative;
        if (relative.empty()) return base;

        // 简单的路径连接（实际项目中应使用 std::filesystem）
        if (base.back() == '/') {
            return base + relative;
        }
        return base + "/" + relative;
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_UTILS_H
