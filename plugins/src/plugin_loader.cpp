/**
 * @file plugin_loader.cpp
 * @brief 插件加载器实现
 */

#include "plugin_loader.h"
#include "service_registry.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace WorkflowSystem {
namespace Plugin {

PluginLoader::PluginLoader()
    : defaultContext_(&defaultContextImpl_) {
}

PluginLoader::PluginLoader(const PluginLoaderConfig& config)
    : config_(config)
    , defaultContext_(&defaultContextImpl_) {
}

PluginLoader::~PluginLoader() {
    unloadAll();
}

void PluginLoader::setConfig(const PluginLoaderConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

PluginLoaderConfig PluginLoader::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void PluginLoader::addSearchPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.searchPaths.push_back(path);
}

void PluginLoader::clearSearchPaths() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.searchPaths.clear();
}

std::vector<std::string> PluginLoader::scanPlugins() {
    std::vector<std::string> result;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& searchPath : config_.searchPaths) {
        DIR* dir = opendir(searchPath.c_str());
        if (!dir) continue;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            
            // 检查文件扩展名
            if (filename.size() > config_.fileExtension.size() &&
                filename.substr(filename.size() - config_.fileExtension.size()) == config_.fileExtension) {
                // 处理路径拼接，避免双斜杠
                std::string fullPath = searchPath;
                if (!fullPath.empty() && fullPath.back() != '/') {
                    fullPath += '/';
                }
                fullPath += filename;
                result.push_back(fullPath);
            }
        }
        closedir(dir);
    }
    
    return result;
}

bool PluginLoader::hasPlugin(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugins_.find(name) != plugins_.end();
}

std::string PluginLoader::getPluginPath(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    return (it != plugins_.end()) ? it->second.path : "";
}

bool PluginLoader::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string resolvedPath = resolvePath(path);
    if (resolvedPath.empty()) {
        return false;
    }
    
    LoadedPlugin info;
    info.path = resolvedPath;
    
    if (!loadLibrary(resolvedPath, info)) {
        return false;
    }
    
    if (!info.createFunc || !info.destroyFunc) {
        unloadLibrary(info);
        return false;
    }
    
    info.instance = info.createFunc();
    if (!info.instance) {
        unloadLibrary(info);
        return false;
    }
    
    info.metadata = info.instance->getMetadata();
    info.state = PluginState::LOADED;
    info.loadTime = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    
    std::string pluginName = info.metadata.name.empty() ? 
                             resolvedPath : info.metadata.name;
    
    plugins_[pluginName] = std::move(info);
    
    fireEvent(pluginName, "loaded");
    return true;
}

int PluginLoader::loadAll() {
    auto pluginPaths = scanPlugins();
    int count = 0;
    
    for (const auto& path : pluginPaths) {
        if (load(path)) {
            count++;
        }
    }
    
    return count;
}

bool PluginLoader::unload(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }
    
    LoadedPlugin& info = it->second;
    
    // 先停止
    if (info.state == PluginState::RUNNING) {
        if (info.instance) {
            info.instance->stop();
        }
    }
    
    // 卸载
    if (info.instance) {
        info.instance->unload();
        info.destroyFunc(info.instance);
        info.instance = nullptr;
    }
    
    unloadLibrary(info);
    
    fireEvent(name, "unloaded");
    plugins_.erase(it);
    
    return true;
}

void PluginLoader::unloadAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : plugins_) {
        LoadedPlugin& info = pair.second;
        
        if (info.state == PluginState::RUNNING && info.instance) {
            info.instance->stop();
        }
        
        if (info.instance) {
            info.instance->unload();
            info.destroyFunc(info.instance);
        }
        
        unloadLibrary(info);
        fireEvent(pair.first, "unloaded");
    }
    
    plugins_.clear();
}

bool PluginLoader::reload(const std::string& name) {
    std::string path = getPluginPath(name);
    if (path.empty()) {
        return false;
    }
    
    if (!unload(name)) {
        return false;
    }
    
    return load(path);
}

bool PluginLoader::initialize(const std::string& name, IPluginContext* context) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }
    
    LoadedPlugin& info = it->second;
    
    if (info.state != PluginState::LOADED) {
        return info.state == PluginState::INITIALIZED || 
               info.state == PluginState::RUNNING;
    }
    
    if (!info.instance) {
        return false;
    }
    
    IPluginContext* ctx = context ? context : defaultContext_;
    
    PluginResult result = info.instance->initialize(ctx);
    if (result.success) {
        info.state = PluginState::INITIALIZED;
        fireEvent(name, "initialized");
        return true;
    }
    
    info.state = PluginState::ERROR;
    return false;
}

int PluginLoader::initializeAll(IPluginContext* context) {
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : plugins_) {
            names.push_back(pair.first);
        }
    }
    
    int count = 0;
    for (const auto& name : names) {
        if (initialize(name, context)) {
            count++;
        }
    }
    
    return count;
}

bool PluginLoader::start(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }
    
    LoadedPlugin& info = it->second;
    
    if (info.state == PluginState::RUNNING) {
        return true;
    }
    
    if (info.state != PluginState::INITIALIZED) {
        return false;
    }
    
    if (!info.instance) {
        return false;
    }
    
    PluginResult result = info.instance->start();
    if (result.success) {
        info.state = PluginState::RUNNING;
        fireEvent(name, "started");
        return true;
    }
    
    info.state = PluginState::ERROR;
    return false;
}

int PluginLoader::startAll() {
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : plugins_) {
            names.push_back(pair.first);
        }
    }
    
    int count = 0;
    for (const auto& name : names) {
        if (start(name)) {
            count++;
        }
    }
    
    return count;
}

bool PluginLoader::stop(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }
    
    LoadedPlugin& info = it->second;
    
    if (info.state != PluginState::RUNNING) {
        return true;
    }
    
    if (!info.instance) {
        return false;
    }
    
    PluginResult result = info.instance->stop();
    if (result.success) {
        info.state = PluginState::STOPPED;
        fireEvent(name, "stopped");
        return true;
    }
    
    return false;
}

int PluginLoader::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int count = 0;
    for (auto& pair : plugins_) {
        LoadedPlugin& info = pair.second;
        if (info.state == PluginState::RUNNING && info.instance) {
            PluginResult result = info.instance->stop();
            if (result.success) {
                info.state = PluginState::STOPPED;
                count++;
            }
        }
    }
    
    return count;
}

IPlugin* PluginLoader::getPlugin(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    return (it != plugins_.end()) ? it->second.instance : nullptr;
}

PluginMetadata PluginLoader::getMetadata(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    return (it != plugins_.end()) ? it->second.metadata : PluginMetadata();
}

PluginState PluginLoader::getState(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    return (it != plugins_.end()) ? it->second.state : PluginState::UNLOADED;
}

std::vector<std::string> PluginLoader::getLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : plugins_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> PluginLoader::getPluginsByState(PluginState state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : plugins_) {
        if (pair.second.state == state) {
            names.push_back(pair.first);
        }
    }
    return names;
}

PluginResult PluginLoader::execute(const std::string& name, const PluginParams& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return PluginResult::error(-1, "Plugin not found: " + name);
    }
    
    LoadedPlugin& info = it->second;
    
    if (info.state != PluginState::RUNNING) {
        return PluginResult::error(-2, "Plugin not running: " + name);
    }
    
    if (!info.instance) {
        return PluginResult::error(-3, "Plugin instance is null: " + name);
    }
    
    return info.instance->execute(params);
}

std::vector<std::pair<std::string, PluginResult>> PluginLoader::executeAll(
    const std::string& action, const PluginParams& params) {
    
    std::vector<std::pair<std::string, PluginResult>> results;
    
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : plugins_) {
            if (pair.second.state == PluginState::RUNNING &&
                pair.second.instance &&
                pair.second.instance->supportsAction(action)) {
                names.push_back(pair.first);
            }
        }
    }
    
    for (const auto& name : names) {
        results.push_back({name, execute(name, params)});
    }
    
    return results;
}

void PluginLoader::setEventCallback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = callback;
}

size_t PluginLoader::getLoadedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugins_.size();
}

int64_t PluginLoader::getTotalLoadTime() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t total = 0;
    for (const auto& pair : plugins_) {
        total += pair.second.loadTime;
    }
    return total;
}

bool PluginLoader::loadLibrary(const std::string& path, LoadedPlugin& info) {
#ifdef _WIN32
    HMODULE handle = LoadLibraryA(path.c_str());
    if (!handle) {
        return false;
    }
    
    info.handle = handle;
    info.createFunc = reinterpret_cast<CreatePluginFunc>(
        GetProcAddress(handle, "createPlugin"));
    info.destroyFunc = reinterpret_cast<DestroyPluginFunc>(
        GetProcAddress(handle, "destroyPlugin"));
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return false;
    }
    
    info.handle = handle;
    info.createFunc = reinterpret_cast<CreatePluginFunc>(
        dlsym(handle, "createPlugin"));
    info.destroyFunc = reinterpret_cast<DestroyPluginFunc>(
        dlsym(handle, "destroyPlugin"));
#endif
    
    return true;
}

void PluginLoader::unloadLibrary(LoadedPlugin& info) {
    if (info.handle) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(info.handle));
#else
        dlclose(info.handle);
#endif
        info.handle = nullptr;
    }
    info.createFunc = nullptr;
    info.destroyFunc = nullptr;
}

std::string PluginLoader::resolvePath(const std::string& nameOrPath) const {
    struct stat st;
    
    // 如果是绝对路径且存在
    if (!nameOrPath.empty() && nameOrPath[0] == '/') {
        if (stat(nameOrPath.c_str(), &st) == 0) {
            return nameOrPath;
        }
    }
    
    // 检查相对路径（从当前工作目录）
    if (stat(nameOrPath.c_str(), &st) == 0) {
        return nameOrPath;
    }
    
    // 在搜索路径中查找
    for (const auto& searchPath : config_.searchPaths) {
        std::string fullPath = searchPath;
        if (!fullPath.empty() && fullPath.back() != '/') {
            fullPath += '/';
        }
        fullPath += nameOrPath;
        
        // 尝试直接路径
        if (stat(fullPath.c_str(), &st) == 0) {
            return fullPath;
        }
        
        // 尝试添加扩展名
        if (fullPath.size() > config_.fileExtension.size() &&
            fullPath.substr(fullPath.size() - config_.fileExtension.size()) != 
            config_.fileExtension) {
            std::string withExt = fullPath + config_.fileExtension;
            if (stat(withExt.c_str(), &st) == 0) {
                return withExt;
            }
        }
        
        // 尝试 lib 前缀
        std::string libPath = searchPath;
        if (!libPath.empty() && libPath.back() != '/') {
            libPath += '/';
        }
        libPath += "lib" + nameOrPath;
        if (stat(libPath.c_str(), &st) == 0) {
            return libPath;
        }
        libPath += config_.fileExtension;
        if (stat(libPath.c_str(), &st) == 0) {
            return libPath;
        }
    }
    
    return "";
}

void PluginLoader::fireEvent(const std::string& pluginName, const std::string& event) {
    if (eventCallback_) {
        eventCallback_(pluginName, event);
    }
}

} // namespace Plugin
} // namespace WorkflowSystem
