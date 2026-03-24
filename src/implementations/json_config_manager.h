/**
 * @file json_config_manager.h
 * @brief 业务流程管理系统 - JSON 配置管理器实现
 *
 * 设计模式：
 * - 单例模式：全局唯一的配置管理器
 * - 观察者模式：配置变更通知
 *
 * 面向对象：
 * - 封装：封装 JSON 配置加载和保存
 */

#ifndef WORKFLOW_SYSTEM_JSON_CONFIG_MANAGER_H
#define WORKFLOW_SYSTEM_JSON_CONFIG_MANAGER_H

#include "interfaces/config_manager.h"
#include "core/logger.h"
#include <mutex>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace WorkflowSystem {

/**
 * @brief 简单的 JSON 解析和生成工具类
 *
 * 注意：这是一个轻量级的 JSON 实现，适用于简单配置场景
 * 对于复杂的 JSON 需求，建议集成 nlohmann/json 或其他专业库
 */
class SimpleJsonParser {
public:
    /**
     * @brief 解析 JSON 字符串为键值对
     * 支持嵌套对象：key.subkey
     */
    static std::map<std::string, std::string> parse(const std::string& json);

    /**
     * @brief 生成 JSON 字符串
     */
    static std::string stringify(const std::map<std::string, std::string>& data);

    /**
     * @brief 解析嵌套键 (如 "workflows.timeout")
     */
    static std::vector<std::string> splitNestedKey(const std::string& key);

private:
    /**
     * @brief 去除空白字符
     */
    static std::string trim(const std::string& str);

    /**
     * @brief 转义 JSON 字符串
     */
    static std::string escapeString(const std::string& str);
};

/**
 * @brief JSON 配置管理器实现
 */
class JsonConfigManager : public IConfigManager {
public:
    static JsonConfigManager& getInstance() {
        static JsonConfigManager instance;
        return instance;
    }

    // IConfigManager 接口实现
    bool load(const std::string& configPath) override;
    bool loadFromString(const std::string& config,
                              ConfigFormat format = ConfigFormat::AUTO) override;
    bool save(const std::string& configPath = "") override;
    std::string toString(ConfigFormat format = ConfigFormat::AUTO) const override;

    std::string getString(const std::string& key,
                                const std::string& defaultValue = "") const override;
    int getInt(const std::string& key,
                     int defaultValue = 0) const override;
    long long getLong(const std::string& key,
                           long long defaultValue = 0) const override;
    double getDouble(const std::string& key,
                           double defaultValue = 0.0) const override;
    bool getBool(const std::string& key,
                      bool defaultValue = false) const override;

    void setString(const std::string& key, const std::string& value) override;
    void setInt(const std::string& key, int value) override;
    void setLong(const std::string& key, long long value) override;
    void setDouble(const std::string& key, double value) override;
    void setBool(const std::string& key, bool value) override;

    bool hasKey(const std::string& key) const override;
    void removeKey(const std::string& key) override;
    void clear() override;

    std::vector<std::string> getAllKeys() const override;
    std::vector<ConfigValidationError> validate() const override;

    void setReloadCallback(ReloadCallback callback) override;
    void addChangeListener(ChangeCallback callback) override;
    void removeChangeListener(const std::string& id) override;

    bool isModified() const override { return modified_; }
    void markSaved() override { modified_ = false; }

    std::string getConfigPath() const override { return configPath_; }

    JsonConfigManager() = default;

protected:
    void notifyChange(const std::string& key,
                           const std::string& oldValue,
                           const std::string& newValue) override;
    void setNestedValue(const std::string& key, const std::string& value);

public:
    virtual ~JsonConfigManager() = default;

    // 删除拷贝构造和赋值
    JsonConfigManager(const JsonConfigManager&) = delete;
    JsonConfigManager& operator=(const JsonConfigManager&) = delete;

    /**
     * @brief 解析嵌套键值 (如 key.subkey -> data["key"]["subkey"])
     */
    std::string getNestedValue(const std::string& key,
                              const std::string& defaultValue = "") const;

private:
    std::map<std::string, std::string> config_;
    std::string configPath_;
    mutable std::mutex mutex_;
    bool modified_ = false;

    ReloadCallback reloadCallback_;
    std::map<std::string, ChangeCallback> changeListeners_;
    int listenerIdCounter_ = 0;
};

/**
 * @brief 配置验证器实现
 */
class ConfigValidator : public IConfigValidator {
public:
    ConfigValidator() = default;
    ~ConfigValidator() = default;

    std::vector<ConfigValidationError> validate(
        const std::map<std::string, std::string>& config) const override;

    void addRule(const ConfigRule& rule) override;
    void clearRules() override;

private:
    std::vector<ConfigRule> rules_;
};

/**
 * @brief 内置验证规则
 */
namespace ConfigRules {
    // 正整数验证
    ConfigRule positiveInteger(const std::string& key,
                                   const std::string& description);

    // 非空字符串验证
    ConfigRule nonEmptyString(const std::string& key,
                               const std::string& description);

    // 路径验证
    ConfigRule validPath(const std::string& key,
                               const std::string& description);

    // 范围验证
    ConfigRule range(const std::string& key,
                         int min, int max,
                         const std::string& description);

    // 枚举验证
    ConfigRule enumeration(const std::string& key,
                               const std::vector<std::string>& allowed,
                               const std::string& description);
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_JSON_CONFIG_MANAGER_H
