#ifndef PLUGIN_FRAMEWORK_CONFIG_MANAGER_HPP
#define PLUGIN_FRAMEWORK_CONFIG_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 配置变更回调
 */
using ConfigChangeCallback = std::function<void(const std::string& key, const std::string& value)>;

/**
 * @brief 配置项
 */
struct ConfigItem {
    std::string key;
    std::string value;
    std::string type;        // 类型: string, int, float, bool
    std::string defaultValue;
    std::string description;
    bool required;
    
    ConfigItem() : required(false) {}
};

/**
 * @brief 配置管理器
 * 
 * 管理框架和插件的配置
 */
class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    /**
     * @brief 加载配置文件
     * @param filePath 配置文件路径
     * @return 成功返回true
     */
    bool loadFromFile(const std::string& filePath);
    
    /**
     * @brief 保存配置到文件
     * @param filePath 配置文件路径
     * @return 成功返回true
     */
    bool saveToFile(const std::string& filePath);
    
    /**
     * @brief 从字符串加载配置（JSON格式）
     * @param content 配置内容
     * @return 成功返回true
     */
    bool loadFromString(const std::string& content);
    
    /**
     * @brief 导出配置为字符串（JSON格式）
     * @return 配置字符串
     */
    std::string toString() const;
    
    /**
     * @brief 设置配置值
     * @param key 配置键（支持分层，如 "plugin.http.port"）
     * @param value 配置值
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取配置值
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    std::string get(const std::string& key, 
                   const std::string& defaultValue = "") const;
    
    /**
     * @brief 获取整数配置值
     */
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    /**
     * @brief 获取浮点数配置值
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    
    /**
     * @brief 获取布尔配置值
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    /**
     * @brief 检查配置键是否存在
     */
    bool has(const std::string& key) const;
    
    /**
     * @brief 移除配置项
     */
    void remove(const std::string& key);
    
    /**
     * @brief 清空所有配置
     */
    void clear();
    
    /**
     * @brief 获取所有配置键
     */
    std::vector<std::string> getKeys() const;
    
    /**
     * @brief 获取指定前缀的所有配置
     * @param prefix 键前缀
     * @return 配置映射
     */
    std::map<std::string, std::string> getByPrefix(const std::string& prefix) const;
    
    /**
     * @brief 注册配置变更监听器
     * @param key 配置键（支持通配符 "*"）
     * @param callback 回调函数
     * @return 监听器ID
     */
    uint64_t watch(const std::string& key, ConfigChangeCallback callback);
    
    /**
     * @brief 移除配置变更监听器
     * @param watcherId 监听器ID
     */
    void unwatch(uint64_t watcherId);
    
    /**
     * @brief 设置配置项定义（用于验证）
     */
    void defineItem(const ConfigItem& item);
    
    /**
     * @brief 验证配置
     * @return 有效返回true
     */
    bool validate() const;
    
    /**
     * @brief 获取配置错误信息
     */
    std::vector<std::string> getValidationErrors() const;
    
private:
    /**
     * @brief 触发配置变更通知
     */
    void notifyChange(const std::string& key, const std::string& value);
    
    /**
     * @brief 解析JSON字符串
     */
    bool parseJson(const std::string& content);
    
    /**
     * @brief 生成JSON字符串
     */
    std::string toJson() const;
    
    std::map<std::string, std::string> config_;
    std::map<std::string, ConfigItem> definitions_;
    
    struct Watcher {
        uint64_t id;
        std::string key;
        ConfigChangeCallback callback;
    };
    
    std::vector<Watcher> watchers_;
    uint64_t nextWatcherId_ = 1;
    
    mutable std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_CONFIG_MANAGER_HPP
