/**
 * @file plugin_interface.h
 * @brief 插件接口定义
 * 
 * 所有插件必须实现此接口才能被插件加载器识别和加载
 */

#ifndef WORKFLOW_SYSTEM_PLUGIN_INTERFACE_H
#define WORKFLOW_SYSTEM_PLUGIN_INTERFACE_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 插件版本信息
 */
struct PluginVersion {
    int major;
    int minor;
    int patch;
    std::string prerelease;
    
    PluginVersion() : major(0), minor(0), patch(0) {}
    PluginVersion(int maj, int min, int pat, const std::string& pre = "")
        : major(maj), minor(min), patch(pat), prerelease(pre) {}
    
    std::string toString() const {
        std::string result = std::to_string(major) + "." + 
                            std::to_string(minor) + "." + 
                            std::to_string(patch);
        if (!prerelease.empty()) {
            result += "-" + prerelease;
        }
        return result;
    }
    
    bool operator>=(const PluginVersion& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch >= other.patch;
    }
    
    bool operator==(const PluginVersion& other) const {
        return major == other.major && 
               minor == other.minor && 
               patch == other.patch;
    }
};

/**
 * @brief 插件元数据
 */
struct PluginMetadata {
    std::string name;               // 插件名称（唯一标识）
    std::string displayName;        // 显示名称
    std::string description;        // 描述
    std::string author;             // 作者
    std::string license;            // 许可证
    std::string website;            // 网站
    PluginVersion version;          // 版本
    PluginVersion minHostVersion;   // 最低宿主版本
    std::vector<std::string> dependencies;  // 依赖的其他插件
    std::vector<std::string> tags;  // 标签
    std::unordered_map<std::string, std::string> extra;  // 额外信息
    
    PluginMetadata() = default;
};

/**
 * @brief 插件状态
 */
enum class PluginState {
    UNLOADED,       // 未加载
    LOADED,         // 已加载
    INITIALIZED,    // 已初始化
    RUNNING,        // 运行中
    STOPPED,        // 已停止
    ERROR           // 错误状态
};

/**
 * @brief 插件配置
 */
struct PluginConfig {
    std::string configPath;         // 配置文件路径
    std::string dataPath;           // 数据目录路径
    std::string tempPath;           // 临时文件路径
    bool enabled;                   // 是否启用
    bool autoStart;                 // 是否自动启动
    int priority;                   // 优先级（数值越大越先加载）
    std::unordered_map<std::string, std::string> settings;  // 配置项
    
    PluginConfig() 
        : enabled(true)
        , autoStart(true)
        , priority(0) {}
};

/**
 * @brief 插件上下文 - 提供插件与宿主交互的能力
 */
class IPluginContext {
public:
    virtual ~IPluginContext() = default;
    
    // ========== 日志 ==========
    virtual void logInfo(const std::string& message) = 0;
    virtual void logWarning(const std::string& message) = 0;
    virtual void logError(const std::string& message) = 0;
    virtual void logDebug(const std::string& message) = 0;
    
    // ========== 配置 ==========
    virtual std::string getConfig(const std::string& key, const std::string& defaultValue = "") const = 0;
    virtual void setConfig(const std::string& key, const std::string& value) = 0;
    
    // ========== 服务注册 ==========
    virtual bool registerService(const std::string& serviceName, void* service) = 0;
    virtual void* getService(const std::string& serviceName) const = 0;
    virtual bool hasService(const std::string& serviceName) const = 0;
    
    // ========== 插件管理 ==========
    virtual std::string getPluginPath() const = 0;
    virtual std::string getDataPath() const = 0;
    virtual std::string getTempPath() const = 0;
    
    // ========== 事件 ==========
    virtual void emitEvent(const std::string& eventName, const std::string& eventData) = 0;
    virtual void subscribeEvent(const std::string& eventName, 
                               std::function<void(const std::string&)> callback) = 0;
};

/**
 * @brief 插件执行结果
 */
struct PluginResult {
    bool success;
    int errorCode;
    std::string message;
    std::string data;
    std::unordered_map<std::string, std::string> extra;
    
    PluginResult() : success(false), errorCode(0) {}
    
    static PluginResult ok(const std::string& msg = "", const std::string& data = "") {
        PluginResult r;
        r.success = true;
        r.message = msg;
        r.data = data;
        return r;
    }
    
    static PluginResult error(int code, const std::string& msg) {
        PluginResult r;
        r.success = false;
        r.errorCode = code;
        r.message = msg;
        return r;
    }
};

/**
 * @brief 插件执行参数
 */
struct PluginParams {
    std::string action;             // 要执行的动作
    std::unordered_map<std::string, std::string> args;      // 字符串参数
    std::unordered_map<std::string, int64_t> intArgs;       // 整数参数
    std::unordered_map<std::string, double> floatArgs;      // 浮点参数
    std::unordered_map<std::string, bool> boolArgs;         // 布尔参数
    std::vector<std::string> listArgs;                       // 列表参数
    void* userData;                 // 用户数据指针
    
    PluginParams() : userData(nullptr) {}
    
    std::string getString(const std::string& key, const std::string& defaultVal = "") const {
        auto it = args.find(key);
        return (it != args.end()) ? it->second : defaultVal;
    }
    
    int64_t getInt(const std::string& key, int64_t defaultVal = 0) const {
        auto it = intArgs.find(key);
        return (it != intArgs.end()) ? it->second : defaultVal;
    }
    
    double getFloat(const std::string& key, double defaultVal = 0.0) const {
        auto it = floatArgs.find(key);
        return (it != floatArgs.end()) ? it->second : defaultVal;
    }
    
    bool getBool(const std::string& key, bool defaultVal = false) const {
        auto it = boolArgs.find(key);
        return (it != boolArgs.end()) ? it->second : defaultVal;
    }
};

/**
 * @brief 插件接口 - 所有插件必须实现
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // ========== 生命周期 ==========
    
    /**
     * @brief 初始化插件
     * @param context 插件上下文
     * @return 初始化结果
     */
    virtual PluginResult initialize(IPluginContext* context) = 0;
    
    /**
     * @brief 启动插件
     * @return 启动结果
     */
    virtual PluginResult start() = 0;
    
    /**
     * @brief 停止插件
     * @return 停止结果
     */
    virtual PluginResult stop() = 0;
    
    /**
     * @brief 卸载插件（清理资源）
     * @return 卸载结果
     */
    virtual PluginResult unload() = 0;
    
    // ========== 信息获取 ==========
    
    /**
     * @brief 获取插件元数据
     */
    virtual PluginMetadata getMetadata() const = 0;
    
    /**
     * @brief 获取插件状态
     */
    virtual PluginState getState() const = 0;
    
    /**
     * @brief 获取插件配置
     */
    virtual PluginConfig getConfig() const = 0;
    
    /**
     * @brief 设置插件配置
     */
    virtual void setConfig(const PluginConfig& config) = 0;
    
    // ========== 功能执行 ==========
    
    /**
     * @brief 执行插件功能
     * @param params 执行参数
     * @return 执行结果
     */
    virtual PluginResult execute(const PluginParams& params) = 0;
    
    /**
     * @brief 获取支持的动作列表
     */
    virtual std::vector<std::string> getSupportedActions() const = 0;
    
    /**
     * @brief 检查是否支持某个动作
     */
    virtual bool supportsAction(const std::string& action) const = 0;
    
    // ========== 健康检查 ==========
    
    /**
     * @brief 健康检查
     */
    virtual PluginResult healthCheck() = 0;
};

// 插件导出宏
#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

// 插件创建函数类型
typedef IPlugin* (*CreatePluginFunc)();
typedef void (*DestroyPluginFunc)(IPlugin*);

// 插件导出辅助宏
#define DEFINE_PLUGIN(PluginClass) \
    extern "C" { \
        PLUGIN_EXPORT ::WorkflowSystem::Plugin::IPlugin* createPlugin() { \
            return new PluginClass(); \
        } \
        PLUGIN_EXPORT void destroyPlugin(::WorkflowSystem::Plugin::IPlugin* plugin) { \
            delete plugin; \
        } \
        PLUGIN_EXPORT const char* getPluginABI() { \
            return "1.0"; \
        } \
    }

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_PLUGIN_INTERFACE_H
