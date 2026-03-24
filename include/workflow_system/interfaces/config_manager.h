/**
 * @file config_manager.h
 * @brief 业务流程管理系统 - 配置管理器接口
 *
 * 设计模式：
 * - 单例模式：全局唯一的配置管理器
 *
 * 面向对象：
 * - 封装：封装配置加载、保存、验证逻辑
 */

#ifndef WORKFLOW_SYSTEM_CONFIG_MANAGER_H
#define WORKFLOW_SYSTEM_CONFIG_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

namespace WorkflowSystem {

/**
 * @brief 配置格式枚举
 */
enum class ConfigFormat {
    JSON,
    YAML,
    INI,
    AUTO      // 自动检测
};

/**
 * @brief 配置验证错误
 */
struct ConfigValidationError {
    std::string key;          // 配置键
    std::string error;        // 错误描述
    std::string suggestion;    // 修复建议（可选）
};

/**
 * @brief 配置变更事件
 */
struct ConfigChangeEvent {
    std::string key;          // 变更的键
    std::string oldValue;      // 旧值
    std::string newValue;      // 新值
};

/**
 * @brief 配置管理器接口
 */
class IConfigManager {
public:
    virtual ~IConfigManager() = default;

    /**
     * @brief 从文件加载配置
     */
    virtual bool load(const std::string& configPath) = 0;

    /**
     * @brief 从字符串加载配置
     */
    virtual bool loadFromString(const std::string& config,
                              ConfigFormat format = ConfigFormat::AUTO) = 0;

    /**
     * @brief 保存配置到文件
     */
    virtual bool save(const std::string& configPath = "") = 0;

    /**
     * @brief 转换为字符串
     */
    virtual std::string toString(ConfigFormat format = ConfigFormat::AUTO) const = 0;

    /**
     * @brief 获取配置值（字符串）
     */
    virtual std::string getString(const std::string& key,
                                const std::string& defaultValue = "") const = 0;

    /**
     * @brief 获取配置值（整数）
     */
    virtual int getInt(const std::string& key,
                     int defaultValue = 0) const = 0;

    /**
     * @brief 获取配置值（长整数）
     */
    virtual long long getLong(const std::string& key,
                           long long defaultValue = 0) const = 0;

    /**
     * @brief 获取配置值（浮点数）
     */
    virtual double getDouble(const std::string& key,
                           double defaultValue = 0.0) const = 0;

    /**
     * @brief 获取配置值（布尔值）
     */
    virtual bool getBool(const std::string& key,
                      bool defaultValue = false) const = 0;

    /**
     * @brief 设置配置值（字符串）
     */
    virtual void setString(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 设置配置值（整数）
     */
    virtual void setInt(const std::string& key, int value) = 0;

    /**
     * @brief 设置配置值（长整数）
     */
    virtual void setLong(const std::string& key, long long value) = 0;

    /**
     * @brief 设置配置值（浮点数）
     */
    virtual void setDouble(const std::string& key, double value) = 0;

    /**
     * @brief 设置配置值（布尔值）
     */
    virtual void setBool(const std::string& key, bool value) = 0;

    /**
     * @brief 检查配置键是否存在
     */
    virtual bool hasKey(const std::string& key) const = 0;

    /**
     * @brief 删除配置键
     */
    virtual void removeKey(const std::string& key) = 0;

    /**
     * @brief 清除所有配置
     */
    virtual void clear() = 0;

    /**
     * @brief 获取所有配置键
     */
    virtual std::vector<std::string> getAllKeys() const = 0;

    /**
     * @brief 验证配置
     */
    virtual std::vector<ConfigValidationError> validate() const = 0;

    /**
     * @brief 设置重载回调
     */
    using ReloadCallback = std::function<void()>;
    virtual void setReloadCallback(ReloadCallback callback) = 0;

    /**
     * @brief 设置配置变更监听器
     */
    using ChangeCallback = std::function<void(const ConfigChangeEvent&)>;
    virtual void addChangeListener(ChangeCallback callback) = 0;

    /**
     * @brief 移除配置变更监听器
     */
    virtual void removeChangeListener(const std::string& id) = 0;

    /**
     * @brief 检查配置是否已修改
     */
    virtual bool isModified() const = 0;

    /**
     * @brief 标记配置为已保存
     */
    virtual void markSaved() = 0;

    /**
     * @brief 获取配置文件路径
     */
    virtual std::string getConfigPath() const = 0;

protected:
    /**
     * @brief 通知配置变更
     */
    virtual void notifyChange(const std::string& key,
                           const std::string& oldValue,
                           const std::string& newValue) = 0;
};

/**
 * @brief 配置规则
 */
struct ConfigRule {
    std::string key;                                          // 配置键
    bool required = false;                                      // 是否必需
    std::function<bool(const std::string&)> validator = nullptr;  // 验证器
    std::string description = "";                             // 描述
    std::string defaultValue = "";                               // 默认值
};

/**
 * @brief 配置验证器
 */
class IConfigValidator {
public:
    virtual ~IConfigValidator() = default;

    /**
     * @brief 验证配置
     */
    virtual std::vector<ConfigValidationError> validate(
        const std::map<std::string, std::string>& config) const = 0;

    /**
     * @brief 添加验证规则
     */
    virtual void addRule(const ConfigRule& rule) = 0;

    /**
     * @brief 清除所有规则
     */
    virtual void clearRules() = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CONFIG_MANAGER_H
