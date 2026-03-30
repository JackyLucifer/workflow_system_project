#ifndef PLUGIN_FRAMEWORK_PLUGIN_INFO_HPP
#define PLUGIN_FRAMEWORK_PLUGIN_INFO_HPP

#include "Version.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 插件状态枚举
 */
enum class PluginState {
    UNLOADED,           // 未加载
    LOADING,            // 加载中
    LOADED,             // 已加载（动态库已加载）
    RESOLVING,          // 解析依赖中
    RESOLVED,           // 依赖已解析
    INITIALIZING,       // 初始化中
    INITIALIZED,        // 已初始化
    STARTING,           // 启动中
    RUNNING,            // 运行中
    PAUSED,             // 已暂停
    STOPPING,           // 停止中
    STOPPED,            // 已停止
    UNLOADING,          // 卸载中
    ERROR,              // 错误状态
    DISABLED            // 已禁用
};

/**
 * @brief 插件状态转换字符串
 */
inline const char* stateToString(PluginState state) {
    switch (state) {
        case PluginState::UNLOADED:      return "未加载";
        case PluginState::LOADING:       return "加载中";
        case PluginState::LOADED:        return "已加载";
        case PluginState::RESOLVING:     return "解析依赖中";
        case PluginState::RESOLVED:      return "依赖已解析";
        case PluginState::INITIALIZING:  return "初始化中";
        case PluginState::INITIALIZED:   return "已初始化";
        case PluginState::STARTING:      return "启动中";
        case PluginState::RUNNING:       return "运行中";
        case PluginState::PAUSED:        return "已暂停";
        case PluginState::STOPPING:      return "停止中";
        case PluginState::STOPPED:       return "已停止";
        case PluginState::UNLOADING:     return "卸载中";
        case PluginState::ERROR:         return "错误";
        case PluginState::DISABLED:      return "已禁用";
        default:                         return "未知";
    }
}

/**
 * @brief 依赖描述
 */
struct Dependency {
    std::string pluginId;              // 依赖的插件ID
    VersionConstraint versionConstraint; // 版本约束
    bool required = true;              // 是否必需（false 表示可选依赖）
    std::string reason;                // 依赖原因说明

    Dependency() = default;

    Dependency(const std::string& id,
             const VersionConstraint& vc,
             bool req = true,
             const std::string& r = "")
        : pluginId(id), versionConstraint(vc)
        , required(req), reason(r) {}

    std::string toString() const {
        return pluginId + " (" + versionConstraint.toString() + ")" +
               (required ? "" : " [可选]");
    }
};

/**
 * @brief 插件能力描述
 */
struct Capability {
    std::string name;                  // 能力名称
    std::string description;           // 能力描述
    std::string version;              // 能力版本

    Capability() = default;

    Capability(const std::string& n, const std::string& desc, const std::string& v = "1.0")
        : name(n), description(desc), version(v) {}
};

/**
 * @brief 插件需求
 */
struct Requirement {
    enum Type {
        HARDWARE,          // 硬件要求
        SOFTWARE,          // 软件要求
        API,              // API要求
        RESOURCE          // 资源要求
    };

    Type type;
    std::string name;                  // 需求名称
    std::string value;                 // 需求值（版本、规格等）
    bool required = true;              // 是否必需

    Requirement() = default;
};

/**
 * @brief 健康状态
 */
struct HealthStatus {
    enum Status {
        HEALTHY,           // 健康
        DEGRADED,         // 降级运行
        UNHEALTHY,        // 不健康
        UNKNOWN           // 未知
    };

    Status status = Status::UNKNOWN;
    std::string message;
    int errorCode = 0;

    bool isHealthy() const { return status == Status::HEALTHY; }
};

/**
 * @brief 插件性能指标
 */
struct PluginMetrics {
    uint64_t loadTimeUs = 0;           // 加载时间（微秒）
    uint64_t initTimeUs = 0;           // 初始化时间（微秒）
    uint64_t memoryUsageBytes = 0;      // 内存使用（字节）
    uint32_t threadCount = 0;          // 线程数
    uint64_t messagesProcessed = 0;     // 已处理消息数
    uint64_t messagesFailed = 0;        // 失败消息数
    uint64_t eventsPublished = 0;       // 已发布事件数
    uint64_t lastErrorTime = 0;         // 最后错误时间
    std::string lastError;             // 最后错误信息

    double getSuccessRate() const {
        auto total = messagesProcessed + messagesFailed;
        return total > 0 ? static_cast<double>(messagesProcessed) / total : 0.0;
    }
};

/**
 * @brief 插件元数据
 */
struct PluginMetadata {
    std::map<std::string, std::string> custom;  // 自定义键值对
};

/**
 * @brief 插件规范（静态信息）
 *
 * 描述插件的基本信息和能力
 */
struct PluginSpec {
    // 基本信息
    std::string id;                    // 唯一标识符（反向域名格式，如: com.example.myplugin）
    std::string name;                  // 显示名称
    std::string description;           // 描述
    Version version;                   // 版本号
    std::string author;                // 作者
    std::string license;               // 许可证
    std::string homepage;              // 主页

    // 依赖关系
    std::vector<Dependency> dependencies;
    std::vector<std::string> conflicts;    // 冲突的插件ID列表

    // 能力与需求
    std::vector<Capability> capabilities;   // 提供的能力
    std::vector<Requirement> requirements;  // 系统需求

    // 元数据
    PluginMetadata metadata;

    // 配置
    std::string defaultConfig;          // 默认配置（JSON字符串）
    std::string configSchema;          // 配置验证schema（JSON Schema）

    // 热重载支持
    bool hotReloadSupported = false;
    std::string migrationScript;      // 数据迁移脚本路径

    // 文件信息（由框架填充）
    std::string libraryPath;           // 插件库文件路径
    std::string configPath;            // 配置文件路径
    std::string resourcePath;          // 资源目录路径

    // 验证插件规范是否有效
    bool isValid() const {
        if (id.empty() || name.empty()) {
            return false;
        }
        // TODO: 更多验证
        return true;
    }

    // 获取完整描述
    std::string getFullDescription() const {
        std::ostringstream oss;
        oss << name << " (" << id << ") v" << version.toString() << "\n"
            << "  作者: " << author << "\n"
            << "  许可证: " << license << "\n"
            << "  描述: " << description;
        return oss.str();
    }
};

/**
 * @brief 插件实例信息（运行时信息）
 */
struct PluginInstanceInfo {
    PluginSpec spec;                    // 插件规范
    PluginState state = PluginState::UNLOADED;  // 当前状态
    std::string stateMessage;           // 状态消息
    HealthStatus healthStatus;         // 健康状态
    PluginMetrics metrics;             // 性能指标

    // 加载信息
    std::string loadPath;              // 加载路径
    void* handle = nullptr;            // 动态库句柄

    // 时间戳
    uint64_t loadTime = 0;             // 加载时间戳
    uint64_t lastErrorTime = 0;        // 最后错误时间

    // 错误信息
    std::string lastError;
    int lastErrorCode = 0;

    bool isRunning() const {
        return state == PluginState::RUNNING;
    }

    bool isError() const {
        return state == PluginState::ERROR;
    }

    bool canExecute() const {
        return isRunning();
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_PLUGIN_INFO_HPP
