#include "workflow_system/plugin/config/ConfigManager.hpp"
#include "workflow_system/plugin/utils/Logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace WorkflowSystem { namespace Plugin {

bool ConfigManager::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        PF_ERROR("无法打开配置文件: " + filePath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return loadFromString(buffer.str());
}

bool ConfigManager::saveToFile(const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        PF_ERROR("无法创建配置文件: " + filePath);
        return false;
    }
    
    file << toString();
    file.close();
    
    PF_INFO("配置已保存到: " + filePath);
    return true;
}

bool ConfigManager::loadFromString(const std::string& content) {
    return parseJson(content);
}

std::string ConfigManager::toString() const {
    return toJson();
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string oldValue;
    auto it = config_.find(key);
    bool changed = (it == config_.end() || it->second != value);
    
    config_[key] = value;
    
    if (changed) {
        notifyChange(key, value);
    }
}

std::string ConfigManager::get(const std::string& key, 
                              const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    std::string value = get(key);
    if (value.empty()) {
        return defaultValue;
    }
    
    try {
        return std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

double ConfigManager::getDouble(const std::string& key, double defaultValue) const {
    std::string value = get(key);
    if (value.empty()) {
        return defaultValue;
    }
    
    try {
        return std::stod(value);
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    std::string value = get(key);
    if (value.empty()) {
        return defaultValue;
    }
    
    // 转换为小写
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

bool ConfigManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.find(key) != config_.end();
}

void ConfigManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.erase(key);
}

void ConfigManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.clear();
}

std::vector<std::string> ConfigManager::getKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    keys.reserve(config_.size());
    
    for (const auto& pair : config_) {
        keys.push_back(pair.first);
    }
    
    return keys;
}

std::map<std::string, std::string> ConfigManager::getByPrefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, std::string> result;
    
    for (const auto& pair : config_) {
        if (pair.first.find(prefix) == 0) {
            result[pair.first] = pair.second;
        }
    }
    
    return result;
}

uint64_t ConfigManager::watch(const std::string& key, ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Watcher watcher;
    watcher.id = nextWatcherId_++;
    watcher.key = key;
    watcher.callback = callback;
    
    watchers_.push_back(watcher);
    
    PF_DEBUG("注册配置监听器: " + key + " ID: " + std::to_string(watcher.id));
    
    return watcher.id;
}

void ConfigManager::unwatch(uint64_t watcherId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    watchers_.erase(
        std::remove_if(watchers_.begin(), watchers_.end(),
                      [watcherId](const Watcher& w) { return w.id == watcherId; }),
        watchers_.end()
    );
}

void ConfigManager::defineItem(const ConfigItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    definitions_[item.key] = item;
}

bool ConfigManager::validate() const {
    return getValidationErrors().empty();
}

std::vector<std::string> ConfigManager::getValidationErrors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> errors;
    
    for (const auto& pair : definitions_) {
        const auto& item = pair.second;
        
        if (item.required && config_.find(item.key) == config_.end()) {
            errors.push_back("缺少必需配置项: " + item.key);
        }
    }
    
    return errors;
}

void ConfigManager::notifyChange(const std::string& key, const std::string& value) {
    for (const auto& watcher : watchers_) {
        // 支持通配符匹配
        if (watcher.key == "*" || watcher.key == key) {
            try {
                watcher.callback(key, value);
            } catch (const std::exception& e) {
                PF_ERROR("配置监听器异常: " + std::string(e.what()));
            }
        }
    }
}

bool ConfigManager::parseJson(const std::string& content) {
    // 简化的JSON解析器（支持基本格式）
    // 实际项目中建议使用nlohmann/json库
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    enum class State {
        START,
        IN_OBJECT,
        IN_KEY,
        AFTER_KEY,
        IN_VALUE,
        IN_STRING,
        IN_NUMBER,
        IN_BOOL,
        AFTER_VALUE
    };
    
    State state = State::START;
    std::string currentKey;
    std::string currentValue;
    bool inEscape = false;
    
    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];
        
        switch (state) {
            case State::START:
                if (c == '{') {
                    state = State::IN_OBJECT;
                }
                break;
                
            case State::IN_OBJECT:
                if (c == '"') {
                    state = State::IN_KEY;
                    currentKey.clear();
                } else if (c == '}') {
                    return true;  // 完成
                }
                break;
                
            case State::IN_KEY:
                if (c == '\\' && !inEscape) {
                    inEscape = true;
                } else if (c == '"' && !inEscape) {
                    state = State::AFTER_KEY;
                } else {
                    currentKey += c;
                    inEscape = false;
                }
                break;
                
            case State::AFTER_KEY:
                if (c == ':') {
                    state = State::IN_VALUE;
                    currentValue.clear();
                }
                break;
                
            case State::IN_VALUE:
                if (c == '"') {
                    state = State::IN_STRING;
                    inEscape = false;
                } else if (std::isdigit(c) || c == '-' || c == '+') {
                    state = State::IN_NUMBER;
                    currentValue += c;
                } else if (c == 't' || c == 'f') {
                    state = State::IN_BOOL;
                    currentValue += c;
                }
                break;
                
            case State::IN_STRING:
                if (c == '\\' && !inEscape) {
                    inEscape = true;
                    currentValue += c;
                } else if (c == '"' && !inEscape) {
                    config_[currentKey] = currentValue;
                    state = State::AFTER_VALUE;
                } else {
                    currentValue += c;
                    inEscape = false;
                }
                break;
                
            case State::IN_NUMBER:
                if (std::isdigit(c) || c == '.') {
                    currentValue += c;
                } else {
                    config_[currentKey] = currentValue;
                    state = State::AFTER_VALUE;
                    if (c == ',') {
                        state = State::IN_OBJECT;
                    }
                }
                break;
                
            case State::IN_BOOL:
                if (std::isalpha(c)) {
                    currentValue += c;
                } else {
                    config_[currentKey] = currentValue;
                    state = State::AFTER_VALUE;
                    if (c == ',') {
                        state = State::IN_OBJECT;
                    }
                }
                break;
                
            case State::AFTER_VALUE:
                if (c == ',') {
                    state = State::IN_OBJECT;
                } else if (c == '}') {
                    return true;
                }
                break;
        }
    }
    
    return state == State::AFTER_VALUE || state == State::IN_OBJECT;
}

std::string ConfigManager::toJson() const {
    std::ostringstream oss;
    oss << "{\n";
    
    bool first = true;
    for (const auto& pair : config_) {
        if (!first) {
            oss << ",\n";
        }
        first = false;
        
        // 简单转义
        std::string escaped = pair.second;
        // TODO: 实现完整的JSON转义
        
        oss << "  \"" << pair.first << "\": \"" << escaped << "\"";
    }
    
    oss << "\n}";
    return oss.str();
}

} // namespace Plugin
} // namespace WorkflowSystem
