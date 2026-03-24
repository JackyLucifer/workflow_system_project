/**
 * @file plugin_config.h
 * @brief 插件配置系统 - 支持JSON配置和热更新
 */

#ifndef WORKFLOW_SYSTEM_PLUGIN_CONFIG_H
#define WORKFLOW_SYSTEM_PLUGIN_CONFIG_H

#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 配置值类型
 */
enum class ConfigType {
    STRING,
    INT,
    DOUBLE,
    BOOL,
    ARRAY,
    OBJECT
};

/**
 * @brief 配置值
 */
struct ConfigValue {
    ConfigType type;
    std::string stringValue;
    int64_t intValue;
    double doubleValue;
    bool boolValue;
    std::vector<ConfigValue> arrayValue;
    std::map<std::string, ConfigValue> objectValue;
    
    ConfigValue() : type(ConfigType::STRING), intValue(0), doubleValue(0.0), boolValue(false) {}
    
    static ConfigValue fromString(const std::string& v) {
        ConfigValue cv;
        cv.type = ConfigType::STRING;
        cv.stringValue = v;
        return cv;
    }
    
    static ConfigValue fromInt(int64_t v) {
        ConfigValue cv;
        cv.type = ConfigType::INT;
        cv.intValue = v;
        return cv;
    }
    
    static ConfigValue fromDouble(double v) {
        ConfigValue cv;
        cv.type = ConfigType::DOUBLE;
        cv.doubleValue = v;
        return cv;
    }
    
    static ConfigValue fromBool(bool v) {
        ConfigValue cv;
        cv.type = ConfigType::BOOL;
        cv.boolValue = v;
        return cv;
    }
    
    std::string asString(const std::string& def = "") const {
        switch (type) {
            case ConfigType::STRING: return stringValue;
            case ConfigType::INT: return std::to_string(intValue);
            case ConfigType::DOUBLE: return std::to_string(doubleValue);
            case ConfigType::BOOL: return boolValue ? "true" : "false";
            default: return def;
        }
    }
    
    int64_t asInt(int64_t def = 0) const {
        switch (type) {
            case ConfigType::INT: return intValue;
            case ConfigType::DOUBLE: return static_cast<int64_t>(doubleValue);
            case ConfigType::STRING: {
                try { return std::stoll(stringValue); }
                catch (...) { return def; }
            }
            case ConfigType::BOOL: return boolValue ? 1 : 0;
            default: return def;
        }
    }
    
    double asDouble(double def = 0.0) const {
        switch (type) {
            case ConfigType::DOUBLE: return doubleValue;
            case ConfigType::INT: return static_cast<double>(intValue);
            case ConfigType::STRING: {
                try { return std::stod(stringValue); }
                catch (...) { return def; }
            }
            default: return def;
        }
    }
    
    bool asBool(bool def = false) const {
        switch (type) {
            case ConfigType::BOOL: return boolValue;
            case ConfigType::INT: return intValue != 0;
            case ConfigType::STRING: 
                return stringValue == "true" || stringValue == "1" || stringValue == "yes";
            default: return def;
        }
    }
};

/**
 * @brief 配置变更回调
 */
using ConfigChangeCallback = std::function<void(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue)>;

/**
 * @brief 插件配置管理器
 */
class PluginConfigManager {
public:
    static PluginConfigManager& getInstance() {
        static PluginConfigManager instance;
        return instance;
    }
    
    // ========== 全局配置 ==========
    
    void setGlobal(const std::string& key, const ConfigValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        ConfigValue oldValue;
        bool hadOldValue = globalConfig_.find(key) != globalConfig_.end();
        if (hadOldValue) {
            oldValue = globalConfig_[key];
        }
        
        globalConfig_[key] = value;
        
        if (hadOldValue && globalChangeCallbacks_.count(key)) {
            for (auto& cb : globalChangeCallbacks_[key]) {
                cb(key, oldValue, value);
            }
        }
    }
    
    ConfigValue getGlobal(const std::string& key, const ConfigValue& def = ConfigValue()) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = globalConfig_.find(key);
        return (it != globalConfig_.end()) ? it->second : def;
    }
    
    std::string getGlobalString(const std::string& key, const std::string& def = "") const {
        return getGlobal(key, ConfigValue::fromString(def)).asString(def);
    }
    
    int64_t getGlobalInt(const std::string& key, int64_t def = 0) const {
        return getGlobal(key, ConfigValue::fromInt(def)).asInt(def);
    }
    
    bool getGlobalBool(const std::string& key, bool def = false) const {
        return getGlobal(key, ConfigValue::fromBool(def)).asBool(def);
    }
    
    bool hasGlobal(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return globalConfig_.find(key) != globalConfig_.end();
    }
    
    bool removeGlobal(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return globalConfig_.erase(key) > 0;
    }
    
    // ========== 插件配置 ==========
    
    void setPluginConfig(const std::string& pluginName, const std::string& key, const ConfigValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        pluginConfigs_[pluginName][key] = value;
    }
    
    ConfigValue getPluginConfig(const std::string& pluginName, const std::string& key, const ConfigValue& def = ConfigValue()) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pluginIt = pluginConfigs_.find(pluginName);
        if (pluginIt == pluginConfigs_.end()) {
            return def;
        }
        auto keyIt = pluginIt->second.find(key);
        return (keyIt != pluginIt->second.end()) ? keyIt->second : def;
    }
    
    std::string getPluginString(const std::string& pluginName, const std::string& key, const std::string& def = "") const {
        return getPluginConfig(pluginName, key, ConfigValue::fromString(def)).asString(def);
    }
    
    int64_t getPluginInt(const std::string& pluginName, const std::string& key, int64_t def = 0) const {
        return getPluginConfig(pluginName, key, ConfigValue::fromInt(def)).asInt(def);
    }
    
    bool getPluginBool(const std::string& pluginName, const std::string& key, bool def = false) const {
        return getPluginConfig(pluginName, key, ConfigValue::fromBool(def)).asBool(def);
    }
    
    std::map<std::string, ConfigValue> getAllPluginConfig(const std::string& pluginName) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pluginConfigs_.find(pluginName);
        return (it != pluginConfigs_.end()) ? it->second : std::map<std::string, ConfigValue>();
    }
    
    bool hasPluginConfig(const std::string& pluginName, const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pluginIt = pluginConfigs_.find(pluginName);
        if (pluginIt == pluginConfigs_.end()) return false;
        return pluginIt->second.find(key) != pluginIt->second.end();
    }
    
    void clearPluginConfig(const std::string& pluginName) {
        std::lock_guard<std::mutex> lock(mutex_);
        pluginConfigs_.erase(pluginName);
    }
    
    // ========== 变更回调 ==========
    
    void onGlobalChange(const std::string& key, ConfigChangeCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        globalChangeCallbacks_[key].push_back(callback);
    }
    
    void onPluginChange(const std::string& pluginName, const std::string& key, ConfigChangeCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        pluginChangeCallbacks_[pluginName][key].push_back(callback);
    }
    
    // ========== JSON导入导出 ==========
    
    std::string toJson() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string json = "{";
        
        json += "\"global\":{";
        bool first = true;
        for (const auto& pair : globalConfig_) {
            if (!first) json += ",";
            json += "\"" + pair.first + "\":" + valueToJson(pair.second);
            first = false;
        }
        json += "},";
        
        json += "\"plugins\":{";
        first = true;
        for (const auto& pluginPair : pluginConfigs_) {
            if (!first) json += ",";
            json += "\"" + pluginPair.first + "\":{";
            bool firstKey = true;
            for (const auto& keyPair : pluginPair.second) {
                if (!firstKey) json += ",";
                json += "\"" + keyPair.first + "\":" + valueToJson(keyPair.second);
                firstKey = false;
            }
            json += "}";
            first = false;
        }
        json += "}";
        
        json += "}";
        return json;
    }
    
    bool fromJson(const std::string& json) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::map<std::string, ConfigValue> newGlobal;
        std::map<std::string, std::map<std::string, ConfigValue>> newPlugins;
        
        if (!parseJson(json, newGlobal, newPlugins)) {
            return false;
        }
        
        globalConfig_ = newGlobal;
        pluginConfigs_ = newPlugins;
        
        return true;
    }
    
    // ========== 管理 ==========
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        globalConfig_.clear();
        pluginConfigs_.clear();
    }
    
    std::vector<std::string> getPluginNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& pair : pluginConfigs_) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    PluginConfigManager() = default;
    
    mutable std::mutex mutex_;
    std::map<std::string, ConfigValue> globalConfig_;
    std::map<std::string, std::map<std::string, ConfigValue>> pluginConfigs_;
    std::map<std::string, std::vector<ConfigChangeCallback>> globalChangeCallbacks_;
    std::map<std::string, std::map<std::string, std::vector<ConfigChangeCallback>>> pluginChangeCallbacks_;
    
    std::string valueToJson(const ConfigValue& value) const {
        switch (value.type) {
            case ConfigType::STRING:
                return "\"" + escapeJson(value.stringValue) + "\"";
            case ConfigType::INT:
                return std::to_string(value.intValue);
            case ConfigType::DOUBLE:
                return std::to_string(value.doubleValue);
            case ConfigType::BOOL:
                return value.boolValue ? "true" : "false";
            case ConfigType::ARRAY: {
                std::string result = "[";
                for (size_t i = 0; i < value.arrayValue.size(); ++i) {
                    if (i > 0) result += ",";
                    result += valueToJson(value.arrayValue[i]);
                }
                result += "]";
                return result;
            }
            case ConfigType::OBJECT: {
                std::string result = "{";
                bool first = true;
                for (const auto& pair : value.objectValue) {
                    if (!first) result += ",";
                    result += "\"" + pair.first + "\":" + valueToJson(pair.second);
                    first = false;
                }
                result += "}";
                return result;
            }
            default:
                return "null";
        }
    }
    
    std::string escapeJson(const std::string& str) const {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
    
    bool parseJson(const std::string& json,
                   std::map<std::string, ConfigValue>& global,
                   std::map<std::string, std::map<std::string, ConfigValue>>& plugins) {
        size_t pos = 0;
        skipWhitespace(json, pos);
        
        if (pos >= json.size() || json[pos] != '{') return false;
        pos++;
        
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            if (json[pos] == '}') break;
            
            std::string key = parseString(json, pos);
            if (key.empty()) return false;
            
            skipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') return false;
            pos++;
            skipWhitespace(json, pos);
            
            if (key == "global") {
                if (!parseObject(json, pos, global)) return false;
            } else if (key == "plugins") {
                size_t pluginsStart = pos;
                if (pos >= json.size() || json[pos] != '{') return false;
                pos++;
                
                while (pos < json.size()) {
                    skipWhitespace(json, pos);
                    if (json[pos] == '}') { pos++; break; }
                    
                    std::string pluginName = parseString(json, pos);
                    if (pluginName.empty()) return false;
                    
                    skipWhitespace(json, pos);
                    if (pos >= json.size() || json[pos] != ':') return false;
                    pos++;
                    skipWhitespace(json, pos);
                    
                    if (!parseObject(json, pos, plugins[pluginName])) return false;
                    
                    skipWhitespace(json, pos);
                    if (pos < json.size() && json[pos] == ',') pos++;
                }
            } else {
                skipValue(json, pos);
            }
            
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') pos++;
        }
        
        return true;
    }
    
    void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || 
               json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }
    
    std::string parseString(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != '"') return "";
        pos++;
        
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                switch (json[pos]) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += json[pos];
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        if (pos < json.size()) pos++;
        return result;
    }
    
    bool parseObject(const std::string& json, size_t& pos, std::map<std::string, ConfigValue>& obj) {
        if (pos >= json.size() || json[pos] != '{') return false;
        pos++;
        
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            if (json[pos] == '}') { pos++; return true; }
            
            std::string key = parseString(json, pos);
            if (key.empty()) return false;
            
            skipWhitespace(json, pos);
            if (pos >= json.size() || json[pos] != ':') return false;
            pos++;
            skipWhitespace(json, pos);
            
            obj[key] = parseValue(json, pos);
            
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') pos++;
        }
        
        return true;
    }
    
    ConfigValue parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        
        if (pos >= json.size()) return ConfigValue();
        
        if (json[pos] == '"') {
            return ConfigValue::fromString(parseString(json, pos));
        }
        
        if (json[pos] == 't' && pos + 3 < json.size() && json.substr(pos, 4) == "true") {
            pos += 4;
            return ConfigValue::fromBool(true);
        }
        
        if (json[pos] == 'f' && pos + 4 < json.size() && json.substr(pos, 5) == "false") {
            pos += 5;
            return ConfigValue::fromBool(false);
        }
        
        if (json[pos] == '-' || (json[pos] >= '0' && json[pos] <= '9')) {
            bool negative = false;
            if (json[pos] == '-') {
                negative = true;
                pos++;
            }
            
            int64_t intVal = 0;
            while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
                intVal = intVal * 10 + (json[pos] - '0');
                pos++;
            }
            
            if (negative) intVal = -intVal;
            return ConfigValue::fromInt(intVal);
        }
        
        return ConfigValue();
    }
    
    void skipValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        
        if (pos >= json.size()) return;
        
        if (json[pos] == '"') {
            parseString(json, pos);
        } else if (json[pos] == '{') {
            pos++;
            int depth = 1;
            while (pos < json.size() && depth > 0) {
                if (json[pos] == '{') depth++;
                else if (json[pos] == '}') depth--;
                pos++;
            }
        } else if (json[pos] == '[') {
            pos++;
            int depth = 1;
            while (pos < json.size() && depth > 0) {
                if (json[pos] == '[') depth++;
                else if (json[pos] == ']') depth--;
                pos++;
            }
        } else {
            while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && 
                   json[pos] != ']' && json[pos] != ' ' && json[pos] != '\n') {
                pos++;
            }
        }
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_PLUGIN_CONFIG_H
