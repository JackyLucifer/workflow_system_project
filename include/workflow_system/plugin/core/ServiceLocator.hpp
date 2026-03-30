#ifndef PLUGIN_FRAMEWORK_SERVICE_LOCATOR_HPP
#define PLUGIN_FRAMEWORK_SERVICE_LOCATOR_HPP

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <typeinfo>
#include <cstdlib>
#include <algorithm>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 服务定位器 - 允许插件获取和注册各种服务
 * 
 * 使用方式:
 * 1. 框架启动时注册服务: ServiceLocator::instance().registerService<ILogger>("logger", loggerPtr);
 * 2. 插件中获取服务: auto logger = context->getService<ILogger>("logger");
 */
class ServiceLocator {
public:
    static ServiceLocator& instance() {
        static ServiceLocator sl;
        return sl;
    }

    // 注册服务（原始指针）
    void registerService(const std::string& name, void* service) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_[name] = service;
    }

    // 注册服务（shared_ptr）
    void registerShared(const std::string& name, std::shared_ptr<void> service) {
        std::lock_guard<std::mutex> lock(mutex_);
        sharedServices_[name] = service;
    }

    // 获取服务（原始指针）
    void* getService(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        return (it != services_.end()) ? it->second : nullptr;
    }

    // 获取服务（shared_ptr）
    std::shared_ptr<void> getShared(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sharedServices_.find(name);
        return (it != sharedServices_.end()) ? it->second : nullptr;
    }

    // 注销服务
    void unregisterService(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.erase(name);
        sharedServices_.erase(name);
    }

    // 检查服务是否存在
    bool hasService(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.find(name) != services_.end() ||
               sharedServices_.find(name) != sharedServices_.end();
    }

    // 获取所有服务名称
    std::vector<std::string> getServiceNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& pair : services_) {
            names.push_back(pair.first);
        }
        for (const auto& pair : sharedServices_) {
            if (std::find(names.begin(), names.end(), pair.first) == names.end()) {
                names.push_back(pair.first);
            }
        }
        return names;
    }

    // 清除所有服务
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
        sharedServices_.clear();
    }

    // 模板方法：注册类型化服务
    template<typename T>
    void registerTyped(const std::string& name, T* service) {
        registerService(name, static_cast<void*>(service));
    }

    // 模板方法：获取类型化服务
    template<typename T>
    T* getTyped(const std::string& name) const {
        return static_cast<T*>(getService(name));
    }

    // 模板方法：注册shared_ptr类型化服务
    template<typename T>
    void registerTypedShared(const std::string& name, std::shared_ptr<T> service) {
        registerShared(name, std::static_pointer_cast<void>(service.get()));
        // 保持引用计数
        sharedPtrHolder_[name] = std::static_pointer_cast<void>(service.get());
    }

    // 模板方法：获取shared_ptr类型化服务
    template<typename T>
    std::shared_ptr<T> getTypedShared(const std::string& name) const {
        auto ptr = getShared(name);
        if (!ptr) return nullptr;
        return std::static_pointer_cast<T>(ptr);
    }

private:
    ServiceLocator() = default;
    ~ServiceLocator() { clear(); }
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;

    std::map<std::string, void*> services_;
    std::map<std::string, std::shared_ptr<void>> sharedServices_;
    std::map<std::string, std::shared_ptr<void>> sharedPtrHolder_;
    mutable std::mutex mutex_;
};

/**
 * @brief 服务工厂 - 用于延迟创建服务
 */
template<typename T>
using ServiceFactory = std::function<std::shared_ptr<T>()>;

/**
 * @brief 可延迟加载的服务定位器
 */
class LazyServiceLocator {
public:
    static LazyServiceLocator& instance() {
        static LazyServiceLocator lsl;
        return lsl;
    }

    // 注册服务工厂
    template<typename T>
    void registerFactory(const std::string& name, ServiceFactory<T> factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_[name] = [factory](void* f) {
            return static_cast<void*>(new std::shared_ptr<T>(((ServiceFactory<T>)f)()));
        };
        rawFactories_[name] = std::move(factory);
    }

    // 获取服务（延迟创建）
    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 先检查缓存
        auto cacheIt = cache_.find(name);
        if (cacheIt != cache_.end()) {
            return std::static_pointer_cast<T>(cacheIt->second);
        }

        // 查找工厂
        auto it = rawFactories_.find(name);
        if (it == rawFactories_.end()) {
            return nullptr;
        }

        // 创建并缓存
        auto& factory = it->second;
        auto service = std::static_pointer_cast<void>(factory());
        cache_[name] = service;
        return std::static_pointer_cast<T>(service);
    }

    // 检查服务是否注册
    bool hasFactory(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return factories_.find(name) != factories_.end();
    }

    // 清除缓存
    void clearCache() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

    // 清除所有
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_.clear();
        rawFactories_.clear();
        cache_.clear();
    }

private:
    LazyServiceLocator() = default;
    ~LazyServiceLocator() { clear(); }
    LazyServiceLocator(const LazyServiceLocator&) = delete;
    LazyServiceLocator& operator=(const LazyServiceLocator&) = delete;

    std::map<std::string, std::function<void*()>> factories_;
    std::map<std::string, std::function<std::shared_ptr<void>()>> rawFactories_;
    std::map<std::string, std::shared_ptr<void>> cache_;
    mutable std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_SERVICE_LOCATOR_HPP
