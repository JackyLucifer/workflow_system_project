/**
 * @file PluginManifest.hpp
 * @brief 插件清单解析器 - 解析plugin.json配置文件
 * 
 * plugin.json 格式示例:
 * {
 *   "id": "com.example.my-plugin",
 *   "name": "MyPlugin",
 *   "version": "1.0.00",
 *   "description": "示例插件",
 *   "author": "开发者",
 *   "main": "libmy_plugin.so",
 *   "dependencies": [
 *     {"id": "com.example.base", "version": ">=1.0.0"}
 *   ],
 *   "config": {
 *     "enabled": true,
 *     "priority": 100
 *   }
 * }
 */

#ifndef PLUGIN_FRAMEWORK_PLUGIN_MANIFEST_HPP
#define PLUGIN_FRAMEWORK_PLUGIN_MANIFEST_HPP

#include "workflow_system/plugin/core/Version.hpp"
#include "workflow_system/plugin/core/PluginInfo.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 依赖项描述
 */
struct DependencyDescriptor {
    std::string id;              // 依赖插件ID
    std::string versionRange;    // 版本范围 (如 ">=1.0.0")
    bool optional;               // 是否可选
    
    DependencyDescriptor() : optional(false) {}
    
    bool operator==(const DependencyDescriptor& other) const {
        return id == other.id && versionRange == other.versionRange;
    }
};

/**
 * @brief 扩展点描述
 */
struct ExtensionDescriptor {
    std::string point;           // 扩展点ID
    std::string className;       // 实现类名
    std::map<std::string, std::string> properties;  // 扩展属性
};

/**
 * @brief 插件清单 - 从plugin.json解析的插件元数据
 */
class PluginManifest {
public:
    PluginManifest() : enabled_(true), priority_(0), autoStart_(true) {}
    
    // 基本信息
    std::string getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }
    
    std::string getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    Version getVersion() const { return version_; }
    void setVersion(const Version& v) { version_ = v; }
    
    std::string getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; }
    
    std::string getAuthor() const { return author_; }
    void setAuthor(const std::string& author) { author_ = author; }
    
    std::string getLicense() const { return license_; }
    void setLicense(const std::string& license) { license_ = license; }
    
    std::string getWebsite() const { return website_; }
    void setWebsite(const std::string& site) { website_ = site; }
    
    // 入口
    std::string getMain() const { return main_; }
    void setMain(const std::string& main) { main_ = main; }
    
    std::string getFactory() const { return factory_; }
    void setFactory(const std::string& factory) { factory_ = factory; }
    
    // 配置
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    int getPriority() const { return priority_; }
    void setPriority(int priority) { priority_ = priority; }
    
    bool isAutoStart() const { return autoStart_; }
    void setAutoStart(bool autoStart) { autoStart_ = autoStart; }
    
    // 依赖
    const std::vector<DependencyDescriptor>& getDependencies() const { return dependencies_; }
    void addDependency(const DependencyDescriptor& dep) { dependencies_.push_back(dep); }
    void setDependencies(const std::vector<DependencyDescriptor>& deps) { dependencies_ = deps; }
    
    // 扩展点
    const std::vector<ExtensionDescriptor>& getExtensions() const { return extensions_; }
    void addExtension(const ExtensionDescriptor& ext) { extensions_.push_back(ext); }
    void setExtensions(const std::vector<ExtensionDescriptor>& exts) { extensions_ = exts; }
    
    // 提供的服务
    const std::vector<std::string>& getProvidedServices() const { return providedServices_; }
    void addProvidedService(const std::string& service) { providedServices_.push_back(service); }
    
    // 自定义配置
    const std::map<std::string, std::string>& getConfig() const { return config_; }
    void setConfig(const std::string& key, const std::string& value) { config_[key] = value; }
    std::string getConfig(const std::string& key, const std::string& def = "") const {
        auto it = config_.find(key);
        return (it != config_.end()) ? it->second : def;
    }
    
    // 路径信息
    std::string getManifestPath() const { return manifestPath_; }
    void setManifestPath(const std::string& path) { manifestPath_ = path; }
    
    std::string getPluginDirectory() const { return pluginDirectory_; }
    void setPluginDirectory(const std::string& dir) { pluginDirectory_ = dir; }
    
    // 转换为PluginSpec
    PluginSpec toPluginSpec() const {
        PluginSpec spec;
        spec.id = id_;
        spec.name = name_;
        spec.version = version_;
        spec.description = description_;
        spec.author = author_;
        spec.license = license_;
        spec.homepage = website_;
        return spec;
    }
    
    // 验证清单
    bool isValid() const {
        return !id_.empty() && !name_.empty() && !main_.empty();
    }
    
    std::string getValidationErrors() const {
        std::string errors;
        if (id_.empty()) errors += "缺少插件ID; ";
        if (name_.empty()) errors += "缺少插件名称; ";
        if (main_.empty()) errors += "缺少入口文件; ";
        return errors;
    }

private:
    std::string id_;
    std::string name_;
    Version version_;
    std::string description_;
    std::string author_;
    std::string license_;
    std::string website_;
    
    std::string main_;
    std::string factory_;
    
    bool enabled_;
    int priority_;
    bool autoStart_;
    
    std::vector<DependencyDescriptor> dependencies_;
    std::vector<ExtensionDescriptor> extensions_;
    std::vector<std::string> providedServices_;
    std::map<std::string, std::string> config_;
    
    std::string manifestPath_;
    std::string pluginDirectory_;
};

/**
 * @brief 插件清单解析器
 */
class PluginManifestParser {
public:
    static std::shared_ptr<PluginManifest> parseFile(const std::string& filePath);
    static std::shared_ptr<PluginManifest> parseString(const std::string& jsonContent);
    static std::string toJson(const PluginManifest& manifest);
    static bool saveToFile(const PluginManifest& manifest, const std::string& filePath);

private:
    static std::string extractJsonValue(const std::string& json, const std::string& key);
};

/**
 * @brief 插件发现器 - 扫描目录查找插件
 */
class PluginDiscoverer {
public:
    static std::vector<std::shared_ptr<PluginManifest>> discover(
        const std::string& directory, 
        bool recursive = true);
    
    static std::vector<std::shared_ptr<PluginManifest>> discoverMultiple(
        const std::vector<std::string>& directories);
    
    static std::string findManifest(const std::string& directory);

private:
    static bool directoryExists(const std::string& path);
    static std::vector<std::string> listDirectories(const std::string& path);
    static std::vector<std::string> listFiles(const std::string& path);
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_PLUGIN_MANIFEST_HPP