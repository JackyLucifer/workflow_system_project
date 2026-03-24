/**
 * @file json_config_manager.cpp
 * @brief 业务流程管理系统 - JSON 配置管理器实现
 */

#include "implementations/json_config_manager.h"
#include <sstream>
#include <fstream>

namespace WorkflowSystem {

// ============================================================================
// SimpleJsonParser 实现
// ============================================================================

std::string SimpleJsonParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string SimpleJsonParser::escapeString(const std::string& str) {
    std::ostringstream result;
    for (char c : str) {
        switch (c) {
            case '"': result << "\\\""; break;
            case '\\': result << "\\\\"; break;
            case '\b': result << "\\b"; break;
            case '\f': result << "\\f"; break;
            case '\n': result << "\\n"; break;
            case '\r': result << "\\r"; break;
            case '\t': result << "\\t"; break;
            default: result << c;
        }
    }
    return result.str();
}

std::vector<std::string> SimpleJsonParser::splitNestedKey(const std::string& key) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : key) {
        if (c == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::map<std::string, std::string> SimpleJsonParser::parse(const std::string& json) {
    std::map<std::string, std::string> result;

    // 简单的键值对解析，支持嵌套对象
    size_t pos = 0;

    // 跳过开头的空白和 {
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos < json.length() && json[pos] == '{') {
        ++pos;
    }

    while (pos < json.length()) {
        // 跳过空白
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
            ++pos;
        }

        if (pos >= json.length()) {
            break;
        }

        // 查找键
        if (json[pos] == '"') {
            ++pos;
            size_t keyEnd = json.find("\"", pos);
            if (keyEnd == std::string::npos) {
                break;
            }
            std::string key = json.substr(pos, keyEnd - pos);
            pos = keyEnd + 1;

            // 跳过冒号和空白
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
                ++pos;
            }
            if (pos < json.length() && json[pos] == ':') {
                ++pos;
            }

            // 解析值
            std::string value;

            if (pos < json.length() && json[pos] == '{') {
                // 嵌套对象，递归解析
                size_t braceEnd = json.find('}', pos);
                if (braceEnd != std::string::npos) {
                    std::string nested = json.substr(pos, braceEnd - pos + 1);
                    auto nestedValues = parse("{" + nested + "}");

                    // 添加前缀
                    for (const auto& pair : nestedValues) {
                        result[key + "." + pair.first] = pair.second;
                    }
                }
                pos = braceEnd + 1;
            } else if (pos < json.length() && json[pos] == '"') {
                ++pos;
                size_t valueEnd = json.find("\"", pos);
                if (valueEnd == std::string::npos) {
                    break;
                }
                value = json.substr(pos, valueEnd - pos);
                pos = valueEnd + 1;
            } else if (pos < json.length() && json[pos] == '[') {
                // 数组，跳过
                ++pos;
                int depth = 1;
                while (pos < json.length() && depth > 0) {
                    if (json[pos] == '[') ++depth;
                    else if (json[pos] == ']') --depth;
                    ++pos;
                }
            } else {
                // 布尔值或数字
                size_t valueEnd = pos;
                while (valueEnd < json.length() &&
                       json[valueEnd] != ',' &&
                       json[valueEnd] != '}' &&
                       json[valueEnd] != '\n') {
                    ++valueEnd;
                }
                value = trim(json.substr(pos, valueEnd - pos));
                pos = valueEnd;
            }

            if (pos < json.length() && (json[pos] == ',' || json[pos] == '}')) {
                ++pos;
            }
        } else if (json[pos] == '}') {
            ++pos;
            break;
        } else {
            ++pos;
        }
    }

    return result;
}

std::string SimpleJsonParser::stringify(const std::map<std::string, std::string>& data) {
    std::ostringstream json;
    json << "{\n";

    bool first = true;
    for (const auto& pair : data) {
        if (!first) {
            json << ",\n";
        }
        first = false;

        std::string key = pair.first;
        std::string value = pair.second;

        // 检查是否是嵌套键
        if (key.find('.') != std::string::npos) {
            auto parts = splitNestedKey(key);
            if (!parts.empty()) {
                json << "  \"" << parts[0] << "\": {\n";

                std::string prefix = "";
                for (size_t i = 1; i < parts.size(); ++i) {
                    json << prefix << "    \"" << parts[i] << "\": ";
                    prefix = ",\n      ";

                    // 检查下一个键是否还包含嵌套
                    size_t nextPos = i + 1;
                    if (nextPos < parts.size()) {
                        json << "{\n";
                        prefix = ",\n        ";
                    } else {
                        // 检查值是否是数字或布尔
                        if (value == "true" || value == "false" ||
                            (value.size() > 0 && (value[0] == '-' || isdigit(value[0])))) {
                            json << value;
                        } else {
                            json << "\"" << escapeString(value) << "\"";
                        }
                    }
                }

                // 闭合所有嵌套对象
                for (size_t i = 1; i < parts.size(); ++i) {
                    json << "\n    }";
                }
                json << "\n  }";
                continue;
            }
        }

        json << "  \"" << key << "\": ";

        // 检查值类型
        if (value.empty()) {
            json << "null";
        } else if (value == "true" || value == "false") {
            json << value;
        } else if (value == "null") {
            json << "null";
        } else {
            // 尝试解析为数字
            bool isNumber = true;
            for (char c : value) {
                if (!isdigit(c) && c != '.' && c != '-' && c != '+') {
                    isNumber = false;
                    break;
                }
            }

            if (isNumber && !value.empty()) {
                json << value;
            } else {
                json << "\"" << escapeString(value) << "\"";
            }
        }
    }

    json << "\n}\n";
    return json.str();
}

// ============================================================================
// JsonConfigManager 实现
// ============================================================================

bool JsonConfigManager::load(const std::string& configPath) {
    std::ifstream file;
    file.open(configPath);

    if (!file.is_open()) {
        LOG_ERROR("[ConfigManager] Failed to open config file: " + configPath);
        return false;
    }

    // C++11 兼容：使用 getline 逐行读取
    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }

    file.close();

    auto loaded = SimpleJsonParser::parse(content);

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = loaded;
    configPath_ = configPath;
    modified_ = false;

    LOG_INFO("[ConfigManager] Loaded config from: " + configPath);

    // 触发重载回调
    if (reloadCallback_) {
        reloadCallback_();
    }
    return true;
}

bool JsonConfigManager::loadFromString(const std::string& config, ConfigFormat format) {
    auto parsed = SimpleJsonParser::parse(config);

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = parsed;
    modified_ = false;

    LOG_INFO("[ConfigManager] Loaded config from string");

    return !config_.empty();
}

bool JsonConfigManager::save(const std::string& configPath) {
    std::string path = configPath.empty() ? configPath_ : configPath;

    std::lock_guard<std::mutex> lock(mutex_);

    std::string jsonContent = SimpleJsonParser::stringify(config_);

    std::ofstream file;
    file.open(path);
    if (!file.is_open()) {
        LOG_ERROR("[ConfigManager] Failed to save config to: " + path);
        return false;
    }

    file << jsonContent;
    file.close();

    modified_ = false;

    LOG_INFO("[ConfigManager] Saved config to: " + path);
    return true;
}

std::string JsonConfigManager::toString(ConfigFormat format) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return SimpleJsonParser::stringify(config_);
}

std::string JsonConfigManager::getString(const std::string& key,
                                        const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string value = getNestedValue(key);
    return value.empty() ? defaultValue : value;
}

int JsonConfigManager::getInt(const std::string& key, int defaultValue) const {
    std::string value = getString(key, "");
    if (value.empty()) {
        return defaultValue;
    }
    try {
        return std::stoi(value);
    } catch (const std::invalid_argument&) {
        LOG_WARNING("[ConfigManager] Invalid integer format for key: " + key);
        return defaultValue;
    } catch (const std::out_of_range&) {
        LOG_WARNING("[ConfigManager] Integer out of range for key: " + key);
        return defaultValue;
    } catch (...) {
        return defaultValue;
    }
}

long long JsonConfigManager::getLong(const std::string& key, long long defaultValue) const {
    std::string value = getString(key, "");
    if (value.empty()) {
        return defaultValue;
    }
    try {
        return std::stoll(value);
    } catch (const std::invalid_argument&) {
        LOG_WARNING("[ConfigManager] Invalid long format for key: " + key);
        return defaultValue;
    } catch (const std::out_of_range&) {
        LOG_WARNING("[ConfigManager] Long out of range for key: " + key);
        return defaultValue;
    } catch (...) {
        return defaultValue;
    }
}

double JsonConfigManager::getDouble(const std::string& key, double defaultValue) const {
    std::string value = getString(key, "");
    if (value.empty()) {
        return defaultValue;
    }
    try {
        return std::stod(value);
    } catch (...) {
        return defaultValue;
    }
}

bool JsonConfigManager::getBool(const std::string& key, bool defaultValue) const {
    std::string value = getString(key, "");
    if (value.empty()) {
        return defaultValue;
    }
    if (value == "true" || value == "1" || value == "yes") {
        return true;
    } else if (value == "false" || value == "0" || value == "no") {
        return false;
    }
    return defaultValue;
}

void JsonConfigManager::setString(const std::string& key, const std::string& value) {
    std::string oldValue = getString(key);

    std::lock_guard<std::mutex> lock(mutex_);
    setNestedValue(key, value);
    modified_ = true;

    notifyChange(key, oldValue, value);
}

void JsonConfigManager::setInt(const std::string& key, int value) {
    setString(key, std::to_string(value));
}

void JsonConfigManager::setLong(const std::string& key, long long value) {
    setString(key, std::to_string(value));
}

void JsonConfigManager::setDouble(const std::string& key, double value) {
    setString(key, std::to_string(value));
}

void JsonConfigManager::setBool(const std::string& key, bool value) {
    setString(key, value ? "true" : "false");
}

bool JsonConfigManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto parts = SimpleJsonParser::splitNestedKey(key);
    if (parts.empty()) {
        auto it = config_.find(key);
        return it != config_.end();
    }

    // 检查嵌套键
    std::string fullKey = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        fullKey += "." + parts[i];
    }

    auto it = config_.find(fullKey);
    return it != config_.end();
}

void JsonConfigManager::removeKey(const std::string& key) {
    std::string oldValue = getString(key);

    std::lock_guard<std::mutex> lock(mutex_);

    // 简单实现：删除所有以 key 开头的键
    auto it = config_.begin();
    while (it != config_.end()) {
        if (it->first == key || it->first.find(key + ".") == 0) {
            it = config_.erase(it);
        } else {
            ++it;
        }
    }

    modified_ = true;

    notifyChange(key, oldValue, "");
}

void JsonConfigManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.clear();
    modified_ = true;

    LOG_INFO("[ConfigManager] Cleared all config");
}

std::vector<std::string> JsonConfigManager::getAllKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> keys;
    for (const auto& pair : config_) {
        // 只返回顶层键（不包含嵌套的）
        if (pair.first.find('.') == std::string::npos) {
            keys.push_back(pair.first);
        }
    }

    return keys;
}

std::vector<ConfigValidationError> JsonConfigManager::validate() const {
    ConfigValidator validator;
    return validator.validate(config_);
}

void JsonConfigManager::setReloadCallback(ReloadCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    reloadCallback_ = callback;
}

void JsonConfigManager::addChangeListener(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string id = "listener_" + std::to_string(listenerIdCounter_);
    ++listenerIdCounter_;

    changeListeners_[id] = callback;

    LOG_INFO("[ConfigManager] Added change listener: " + id);
}

void JsonConfigManager::removeChangeListener(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = changeListeners_.find(id);
    if (it != changeListeners_.end()) {
        changeListeners_.erase(it);
        LOG_INFO("[ConfigManager] Removed change listener: " + id);
    }
}

void JsonConfigManager::notifyChange(const std::string& key,
                                    const std::string& oldValue,
                                    const std::string& newValue) {
    ConfigChangeEvent event;
    event.key = key;
    event.oldValue = oldValue;
    event.newValue = newValue;

    for (const auto& pair : changeListeners_) {
        try {
            pair.second(event);
        } catch (const std::exception& e) {
            LOG_ERROR("[ConfigManager] Error in change listener: " +
                      std::string(e.what()));
        } catch (...) {
            LOG_ERROR("[ConfigManager] Unknown error in change listener");
        }
    }
}

std::string JsonConfigManager::getNestedValue(const std::string& key,
                                            const std::string& defaultValue) const {
    auto parts = SimpleJsonParser::splitNestedKey(key);
    if (parts.empty()) {
        auto it = config_.find(key);
        return it != config_.end() ? it->second : defaultValue;
    }

    // 查找嵌套值
    std::string fullKey = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        fullKey += "." + parts[i];
    }

    auto it = config_.find(fullKey);
    return it != config_.end() ? it->second : defaultValue;
}

void JsonConfigManager::setNestedValue(const std::string& key, const std::string& value) {
    auto parts = SimpleJsonParser::splitNestedKey(key);
    if (parts.empty()) {
        config_[key] = value;
        return;
    }

    // 设置嵌套值
    std::string fullKey = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        fullKey += "." + parts[i];
    }

    config_[fullKey] = value;
}

// ============================================================================
// ConfigValidator 实现
// ============================================================================

std::vector<ConfigValidationError> ConfigValidator::validate(
    const std::map<std::string, std::string>& config) const {

    std::vector<ConfigValidationError> errors;

    for (const auto& rule : rules_) {
        auto it = config.find(rule.key);
        if (it != config.end()) {
            if (rule.validator) {
                if (!rule.validator(it->second)) {
                    ConfigValidationError error;
                    error.key = rule.key;
                    error.error = rule.description;
                    errors.push_back(error);
                }
            }
        } else if (rule.required) {
            ConfigValidationError error;
            error.key = rule.key;
            error.error = "Required key is missing";
            error.suggestion = "Add: " + rule.key;
            errors.push_back(error);
        }
    }

    return errors;
}

void ConfigValidator::addRule(const ConfigRule& rule) {
    rules_.push_back(rule);
}

void ConfigValidator::clearRules() {
    rules_.clear();
}

// ============================================================================
// 内置验证规则实现
// ============================================================================

namespace ConfigRules {

ConfigRule positiveInteger(const std::string& key,
                               const std::string& description) {
    ConfigRule rule;
    rule.key = key;
    rule.description = description;
    rule.validator = [](const std::string& value) -> bool {
        try {
            int num = std::stoi(value);
            return num > 0;
        } catch (...) {
            return false;
        }
    };
    return rule;
}

ConfigRule nonEmptyString(const std::string& key,
                           const std::string& description) {
    ConfigRule rule;
    rule.key = key;
    rule.description = description;
    rule.required = true;
    rule.validator = [](const std::string& value) -> bool {
        return !value.empty();
    };
    return rule;
}

ConfigRule validPath(const std::string& key,
                       const std::string& description) {
    ConfigRule rule;
    rule.key = key;
    rule.description = description;
    rule.validator = [](const std::string& value) -> bool {
        // 简单验证：不以空格开头，不包含特殊字符
        if (value.empty()) return false;
        if (value[0] == ' ' || value[0] == '\t') return false;
        if (value.find_first_of("<>\"|?*") != std::string::npos) return false;
        return true;
    };
    return rule;
}

ConfigRule range(const std::string& key,
                     int min, int max,
                     const std::string& description) {
    ConfigRule rule;
    rule.key = key;
    rule.description = description;
    rule.validator = [min, max](const std::string& value) -> bool {
        try {
            int num = std::stoi(value);
            return num >= min && num <= max;
        } catch (...) {
            return false;
        }
    };
    return rule;
}

ConfigRule enumeration(const std::string& key,
                       const std::vector<std::string>& allowed,
                       const std::string& description) {
    ConfigRule rule;
    rule.key = key;
    rule.description = description;
    rule.validator = [allowed](const std::string& value) -> bool {
        return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
    };
    return rule;
}

} // namespace ConfigRules

} // namespace WorkflowSystem
