#include "workflow_system/plugin/runtime/PluginManager.hpp"
#include "workflow_system/core/logger.h"
#include <dlfcn.h>
#include <stdexcept>
#include <algorithm>
#include <queue>
#include <sstream>

namespace WorkflowSystem { namespace Plugin {

PluginManager::PluginManager()
    : PluginManager("./plugins") {}

PluginManager::PluginManager(const std::string& pluginDirectory)
    : pluginDirectory_(pluginDirectory)
    , configDirectory_("./config")
    , dataDirectory_("./data") {

    eventBus_ = std::unique_ptr<EventBus>(new EventBus());
    messageBus_ = std::unique_ptr<MessageBus>(new MessageBus());
}

PluginManager::~PluginManager() {
    shutdown();
}

// ==================== 插件加载 ====================

LoadResult PluginManager::loadPlugin(const std::string& pluginPath) {
    return loadPluginInternal(pluginPath);
}

LoadResult PluginManager::loadPlugin(const PluginSpec& spec,
                                    const std::string& libraryPath) {
    return loadPluginInternal(libraryPath, &spec);
}

int PluginManager::loadPluginsFromDirectory(const std::string& directory,
                                           bool recursive) {
    int loadedCount = 0;

    // 简化实现：实际应该遍历目录
    // 这里假设我们知道某些插件路径

    LOG_INFO("从目录加载插件: " + directory);

    return loadedCount;
}

int PluginManager::loadPluginsFromConfig(const std::string& configFile) {
    // 简化实现：实际应该解析配置文件
    LOG_INFO("从配置文件加载插件: " + configFile);
    return 0;
}

// ==================== 插件卸载 ====================

UnloadResult PluginManager::unloadPlugin(const std::string& pluginId) {
    return unloadPluginInternal(pluginId, false);
}

UnloadResult PluginManager::forceUnloadPlugin(const std::string& pluginId) {
    return unloadPluginInternal(pluginId, true);
}

int PluginManager::unloadAllPlugins() {
    std::vector<std::string> pluginIds;

    {
        std::lock_guard<std::mutex> lock(pluginsMutex_);
        for (const auto& pair : plugins_) {
            pluginIds.push_back(pair.first);
        }
    }

    int count = 0;
    for (const auto& pluginId : pluginIds) {
        if (unloadPlugin(pluginId).success) {
            count++;
        }
    }

    return count;
}

// ==================== 插件重载 ====================

LoadResult PluginManager::reloadPlugin(const std::string& pluginId) {
    std::string libraryPath;
    PluginSpec spec;

    {
        std::lock_guard<std::mutex> lock(pluginsMutex_);
        auto it = plugins_.find(pluginId);
        if (it == plugins_.end()) {
            return LoadResult::error("Plugin not found: " + pluginId);
        }

        libraryPath = it->second->libraryPath;
        spec = it->second->spec;
    }

    // 卸载插件
    unloadPlugin(pluginId);

    // 重新加载
    return loadPlugin(spec, libraryPath);
}

bool PluginManager::hotReloadPlugin(const std::string& pluginId) {
    // 简化实现：实际应该检测文件变化并热重载
    return reloadPlugin(pluginId).success;
}

// ==================== 插件启用/禁用 ====================

bool PluginManager::enablePlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return false;
    }

    auto& entry = *it->second;

    if (entry.state == PluginState::DISABLED) {
        return transitionState(entry, PluginState::LOADED);
    }

    return false;
}

bool PluginManager::disablePlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return false;
    }

    auto& entry = *it->second;

    // 停止插件
    if (entry.state == PluginState::RUNNING) {
        entry.plugin->onStop();
    }

    return transitionState(entry, PluginState::DISABLED);
}

// ==================== 插件查询 ====================

IPlugin* PluginManager::getPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    return (it != plugins_.end()) ? it->second->plugin.get() : nullptr;
}

const IPlugin* PluginManager::getPlugin(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    return (it != plugins_.end()) ? it->second->plugin.get() : nullptr;
}

std::vector<IPlugin*> PluginManager::getAllPlugins() {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    std::vector<IPlugin*> result;
    for (auto& pair : plugins_) {
        if (pair.second->plugin) {
            result.push_back(pair.second->plugin.get());
        }
    }

    return result;
}

std::vector<const IPlugin*> PluginManager::getAllPlugins() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    std::vector<const IPlugin*> result;
    for (const auto& pair : plugins_) {
        if (pair.second->plugin) {
            result.push_back(pair.second->plugin.get());
        }
    }

    return result;
}

std::vector<PluginSpec> PluginManager::getLoadedPluginSpecs() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    std::vector<PluginSpec> result;
    for (const auto& pair : plugins_) {
        result.push_back(pair.second->spec);
    }

    return result;
}

PluginInstanceInfo PluginManager::getPluginInfo(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return PluginInstanceInfo{};
    }

    const auto& entry = it->second;

    PluginInstanceInfo info;
    info.spec = entry->spec;
    info.state = entry->state;
    info.loadTime = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    info.healthStatus = entry->plugin ? entry->plugin->getHealthStatus() : HealthStatus{};
    info.metrics = entry->plugin ? entry->plugin->getMetrics() : PluginMetrics{};

    return info;
}

std::vector<PluginInstanceInfo> PluginManager::getAllPluginInfo() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    std::vector<PluginInstanceInfo> result;

    for (const auto& pair : plugins_) {
        result.push_back(getPluginInfo(pair.first));
    }

    return result;
}

// ==================== 插件状态 ====================

bool PluginManager::isPluginLoaded(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return false;
    }

    auto state = it->second->state;
    return state != PluginState::UNLOADED &&
           state != PluginState::ERROR &&
           state != PluginState::UNLOADING;
}

bool PluginManager::isPluginRunning(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return false;
    }

    return it->second->state == PluginState::RUNNING;
}

PluginState PluginManager::getPluginState(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return PluginState::UNLOADED;
    }

    return it->second->state;
}

// ==================== 插件调用 ====================

std::string PluginManager::callPlugin(const std::string& pluginId,
                                     const std::string& method,
                                     const std::string& params) {
    auto* plugin = getPlugin(pluginId);
    if (!plugin) {
        return "{\"error\": \"Plugin not found\"}";
    }

    try {
        return plugin->callMethod(method, params);
    } catch (const std::exception& e) {
        return "{\"error\": \"" + std::string(e.what()) + "\"}";
    }
}

Response PluginManager::sendPluginMessage(const std::string& pluginId,
                                         const Message& message) {
    auto* plugin = getPlugin(pluginId);
    if (!plugin) {
        return Response::error("Plugin not found");
    }

    try {
        return plugin->onMessage(message);
    } catch (const std::exception& e) {
        return Response::error(std::string("Message error: ") + e.what());
    }
}

// ==================== 批量操作 ====================

int PluginManager::startAllPlugins() {
    int count = 0;

    std::lock_guard<std::mutex> lock(pluginsMutex_);

    for (auto& pair : plugins_) {
        auto& entry = *pair.second;

        if (entry.state == PluginState::LOADED || entry.state == PluginState::INITIALIZED) {
            if (entry.plugin->onStart()) {
                transitionState(entry, PluginState::RUNNING);
                count++;
            }
        }
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.runningPlugins = count;
    }

    return count;
}

int PluginManager::stopAllPlugins() {
    int count = 0;

    std::lock_guard<std::mutex> lock(pluginsMutex_);

    for (auto& pair : plugins_) {
        auto& entry = *pair.second;

        if (entry.state == PluginState::RUNNING) {
            entry.plugin->onStop();
            transitionState(entry, PluginState::STOPPED);
            count++;
        }
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.runningPlugins = 0;
    }

    return count;
}

int PluginManager::pauseAllPlugins() {
    // 简化实现
    return 0;
}

int PluginManager::resumeAllPlugins() {
    // 简化实现
    return 0;
}

// ==================== 依赖管理 ====================

bool PluginManager::resolveDependencies(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        return false;
    }

    return checkDependencies(it->second->spec);
}

std::map<std::string, std::vector<std::string>>
PluginManager::getDependencyGraph() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    std::map<std::string, std::vector<std::string>> graph;

    for (const auto& pair : plugins_) {
        std::vector<std::string> deps;
        for (const auto& dep : pair.second->spec.dependencies) {
            deps.push_back(dep.pluginId);
        }
        graph[pair.first] = deps;
    }

    return graph;
}

std::vector<std::string> PluginManager::getLoadOrder() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);

    std::vector<std::string> order;

    for (const auto& pair : plugins_) {
        order.push_back(pair.first);
    }

    return order;
}

// ==================== 系统信息 ====================

PluginManagerStatistics PluginManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    PluginManagerStatistics stats = statistics_;

    {
        std::lock_guard<std::mutex> pluginsLock(pluginsMutex_);
        stats.totalPlugins = plugins_.size();

        for (const auto& pair : plugins_) {
            if (pair.second->state == PluginState::RUNNING) {
                stats.loadedPlugins++;
            }
            if (pair.second->state == PluginState::ERROR) {
                stats.errorPlugins++;
            }
        }
    }

    return stats;
}

std::string PluginManager::getSystemStatus() const {
    auto stats = getStatistics();

    std::ostringstream oss;
    oss << "插件系统状态:\n"
        << "  总插件数: " << stats.totalPlugins << "\n"
        << "  已加载: " << stats.loadedPlugins << "\n"
        << "  运行中: " << stats.runningPlugins << "\n"
        << "  错误: " << stats.errorPlugins;

    return oss.str();
}

std::string PluginManager::getHealthReport() const {
    auto stats = getStatistics();
    return getSystemStatus();
}

// ==================== 事件和消息 ====================

EventResult PluginManager::broadcastEvent(const Event& event) {
    if (!eventBus_) {
        return EventResult{};
    }

    return eventBus_->publish(event);
}

void PluginManager::broadcastMessage(const Message& message) {
    if (!messageBus_) {
        return;
    }

    messageBus_->broadcast(message);
}

// ==================== 配置 ====================

std::string PluginManager::getPluginDirectory() const {
    return pluginDirectory_;
}

void PluginManager::setPluginDirectory(const std::string& directory) {
    pluginDirectory_ = directory;
}

std::string PluginManager::getConfigDirectory() const {
    return configDirectory_;
}

void PluginManager::setConfigDirectory(const std::string& directory) {
    configDirectory_ = directory;
}

std::string PluginManager::getDataDirectory() const {
    return dataDirectory_;
}

void PluginManager::setDataDirectory(const std::string& directory) {
    dataDirectory_ = directory;
}

// ==================== 热加载 ====================

bool PluginManager::enableHotReload(const std::string& directory) {
    hotReloadEnabled_.store(true);
    LOG_INFO("热加载已启用: " + directory);
    return true;
}

void PluginManager::disableHotReload() {
    hotReloadEnabled_.store(false);
    LOG_INFO("热加载已禁用");
}

bool PluginManager::isHotReloadEnabled() const {
    return hotReloadEnabled_.load();
}

// ==================== 额外功能 ====================

bool PluginManager::initialize() {
    LOG_INFO("初始化插件管理器");

    // 启动通信总线
    eventBus_->startAsyncProcessing();
    messageBus_->startBackgroundProcessing();

    return true;
}

void PluginManager::shutdown() {
    LOG_INFO("关闭插件管理器");

    // 停止所有插件
    stopAllPlugins();

    // 卸载所有插件
    unloadAllPlugins();

    // 停止通信总线
    if (eventBus_) {
        eventBus_->stopAsyncProcessing();
    }

    if (messageBus_) {
        messageBus_->stopBackgroundProcessing();
    }
}

IEventBus* PluginManager::getEventBus() {
    return eventBus_.get();
}

IMessageBus* PluginManager::getMessageBus() {
    return messageBus_.get();
}

// ==================== 内部方法 ====================

LoadResult PluginManager::loadPluginInternal(const std::string& pluginPath,
                                            const PluginSpec* externalSpec) {
    // 加载动态库
    void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);
    if (!handle) {
        return LoadResult::error("无法加载库: " + std::string(dlerror()));
    }

    // 获取创建函数
    auto createFunc = reinterpret_cast<IPlugin* (*)()>(dlsym(handle, "createPlugin"));
    if (!createFunc) {
        dlclose(handle);
        return LoadResult::error("找不到createPlugin函数");
    }

    // 获取销毁函数
    auto destroyFunc = reinterpret_cast<void (*)(IPlugin*)>(dlsym(handle, "destroyPlugin"));
    if (!destroyFunc) {
        dlclose(handle);
        return LoadResult::error("找不到destroyPlugin函数");
    }

    // 创建插件实例
    IPlugin* plugin = createFunc();
    if (!plugin) {
        dlclose(handle);
        return LoadResult::error("创建插件实例失败");
    }

    // 获取插件规范
    PluginSpec spec = externalSpec ? *externalSpec : plugin->getSpec();
    std::string pluginId = spec.id;

    // 创建插件条目
    std::unique_ptr<PluginEntry> entry(new PluginEntry());
    entry->pluginId = pluginId;
    entry->spec = spec;
    entry->plugin.reset(plugin);  // 使用自定义删除器
    entry->libraryPath = pluginPath;
    entry->libraryHandle = handle;
    entry->createFunc = createFunc;
    entry->destroyFunc = destroyFunc;
    entry->state = PluginState::LOADED;

    // 创建插件上下文
    entry->context = std::unique_ptr<PluginContext>(new PluginContext(
        pluginId,
        spec,
        this,
        eventBus_.get(),
        messageBus_.get()
    ));

    // 检查依赖
    if (!checkDependencies(spec)) {
        cleanupPluginEntry(*entry);
        return LoadResult::error("依赖检查失败");
    }

    {
        std::lock_guard<std::mutex> lock(pluginsMutex_);
        plugins_[pluginId] = std::move(entry);
    }

    // 调用onLoad
    auto* ctx = plugins_[pluginId]->context.get();
    if (!plugin->onLoad(ctx)) {
        unloadPlugin(pluginId);
        return LoadResult::error("插件onLoad失败");
    }

    // 调用onInitialize
    if (!plugin->onInitialize()) {
        unloadPlugin(pluginId);
        return LoadResult::error("插件onInitialize失败");
    }

    // 自动启动
    if (!plugin->onStart()) {
        LOG_WARNING("插件onStart失败，插件将保持已加载状态");
    } else {
        std::lock_guard<std::mutex> lock(pluginsMutex_);
        plugins_[pluginId]->state = PluginState::RUNNING;
    }

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.loadedPlugins++;
    }

    LOG_INFO("插件加载成功: " + pluginId);

    return LoadResult::ok(pluginId);
}

UnloadResult PluginManager::unloadPluginInternal(const std::string& pluginId, bool force) {
    std::unique_ptr<PluginEntry> entry;

    {
        std::lock_guard<std::mutex> lock(pluginsMutex_);
        auto it = plugins_.find(pluginId);
        if (it == plugins_.end()) {
            return UnloadResult::error("插件未找到");
        }

        entry = std::move(it->second);
        plugins_.erase(it);
    }

    // 检查是否有其他插件依赖此插件
    if (!force) {
        auto deps = getDependencyGraph();
        for (const auto& pair : deps) {
            for (const auto& dep : pair.second) {
                if (dep == pluginId) {
                    return UnloadResult::error("插件被其他插件依赖");
                }
            }
        }
    }

    // 停止插件
    if (entry->plugin && entry->state == PluginState::RUNNING) {
        entry->plugin->onStop();
    }

    // 清理插件
    entry->plugin->onCleanup();
    entry->plugin->onUnload();

    cleanupPluginEntry(*entry);

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_.loadedPlugins--;
    }

    LOG_INFO("插件卸载成功: " + pluginId);

    return UnloadResult::ok();
}

bool PluginManager::transitionState(PluginEntry& entry, PluginState newState) {
    // 简单的状态转换验证
    switch (entry.state) {
        case PluginState::UNLOADED:
            if (newState == PluginState::LOADING) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::LOADING:
            if (newState == PluginState::LOADED || newState == PluginState::ERROR) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::LOADED:
            if (newState == PluginState::INITIALIZING || newState == PluginState::DISABLED) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::INITIALIZING:
            if (newState == PluginState::INITIALIZED || newState == PluginState::ERROR) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::INITIALIZED:
            if (newState == PluginState::STARTING) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::STARTING:
            if (newState == PluginState::RUNNING || newState == PluginState::ERROR) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::RUNNING:
            if (newState == PluginState::STOPPING || newState == PluginState::PAUSED) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::STOPPING:
            if (newState == PluginState::STOPPED) {
                entry.state = newState;
                return true;
            }
            break;

        case PluginState::STOPPED:
        case PluginState::DISABLED:
        case PluginState::ERROR:
            if (newState == PluginState::UNLOADING) {
                entry.state = newState;
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

bool PluginManager::checkDependencies(const PluginSpec& spec) {
    // 检查所有依赖是否已加载
    for (const auto& dep : spec.dependencies) {
        bool found = false;

        for (const auto& pair : plugins_) {
            if (pair.second->spec.id == dep.pluginId) {
                found = true;
                // 检查版本
                if (!dep.versionConstraint.satisfies(pair.second->spec.version)) {
                    LOG_ERROR("依赖版本不匹配: " + dep.pluginId);
                    return false;
                }
                break;
            }
        }

        if (!found && dep.required) {
            LOG_ERROR("缺少必需依赖: " + dep.pluginId);
            return false;
        }
    }

    return true;
}

std::vector<std::string> PluginManager::calculateLoadOrder(const std::string& pluginId) {
    // 简化实现：使用拓扑排序
    std::vector<std::string> order;
    order.push_back(pluginId);
    return order;
}

void PluginManager::cleanupPluginEntry(PluginEntry& entry) {
    // 关闭上下文
    if (entry.context) {
        entry.context->shutdown();
        entry.context.reset();
    }

    // 卸载动态库
    if (entry.libraryHandle) {
        dlclose(entry.libraryHandle);
        entry.libraryHandle = nullptr;
    }
}

} // namespace Plugin
} // namespace WorkflowSystem
