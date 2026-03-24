/**
 * @file service_registry.h
 * @brief 服务注册中心 - 支持宿主向插件注入服务
 */

#ifndef WORKFLOW_SYSTEM_SERVICE_REGISTRY_H
#define WORKFLOW_SYSTEM_SERVICE_REGISTRY_H

#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <functional>
#include <typeinfo>
#include <type_traits>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 服务句柄 - 类型擦除的服务包装
 */
class ServiceHandle {
public:
    ServiceHandle() : ptr_(nullptr), deleter_(nullptr) {}
    
    template<typename T>
    explicit ServiceHandle(T* ptr, bool owned = false)
        : ptr_(ptr)
        , deleter_(nullptr)
        , typeName_(typeid(T).name()) {
        if (owned) {
            deleter_ = [](void* p) { delete static_cast<T*>(p); };
        }
    }
    
    template<typename T>
    explicit ServiceHandle(std::shared_ptr<T> ptr)
        : ptr_(ptr.get())
        , deleter_(nullptr)
        , typeName_(typeid(T).name())
        , sharedPtr_(ptr) {}
    
    ~ServiceHandle() {
        if (deleter_ && ptr_) {
            deleter_(ptr_);
        }
    }
    
    ServiceHandle(const ServiceHandle&) = delete;
    ServiceHandle& operator=(const ServiceHandle&) = delete;
    
    ServiceHandle(ServiceHandle&& other) noexcept
        : ptr_(other.ptr_)
        , deleter_(other.deleter_)
        , typeName_(std::move(other.typeName_))
        , sharedPtr_(std::move(other.sharedPtr_)) {
        other.ptr_ = nullptr;
        other.deleter_ = nullptr;
    }
    
    ServiceHandle& operator=(ServiceHandle&& other) noexcept {
        if (this != &other) {
            if (deleter_ && ptr_) {
                deleter_(ptr_);
            }
            ptr_ = other.ptr_;
            deleter_ = other.deleter_;
            typeName_ = std::move(other.typeName_);
            sharedPtr_ = std::move(other.sharedPtr_);
            other.ptr_ = nullptr;
            other.deleter_ = nullptr;
        }
        return *this;
    }
    
    template<typename T>
    T* get() const {
        return static_cast<T*>(ptr_);
    }
    
    void* getRaw() const { return ptr_; }
    const std::string& typeName() const { return typeName_; }
    bool valid() const { return ptr_ != nullptr; }
    std::shared_ptr<void> getShared() const { return sharedPtr_; }
    
private:
    void* ptr_;
    void (*deleter_)(void*);
    std::string typeName_;
    std::shared_ptr<void> sharedPtr_;
};

/**
 * @brief 服务工厂 - 延迟创建服务
 */
class ServiceFactory {
public:
    using FactoryFunc = std::function<std::shared_ptr<void>()>;
    
    ServiceFactory() = default;
    explicit ServiceFactory(FactoryFunc func) : factory_(std::move(func)) {}
    
    std::shared_ptr<void> create() const {
        if (factory_) {
            return factory_();
        }
        return nullptr;
    }
    
    bool valid() const { return static_cast<bool>(factory_); }
    
private:
    FactoryFunc factory_;
};

/**
 * @brief 服务注册项
 */
struct ServiceEntry {
    std::string name;
    std::string typeName;
    ServiceHandle instance;
    ServiceFactory factory;
    bool singleton;
    bool initialized;
    
    ServiceEntry() : singleton(true), initialized(false) {}
};

/**
 * @brief 服务注册中心
 */
class ServiceRegistry {
public:
    static ServiceRegistry& getInstance() {
        static ServiceRegistry instance;
        return instance;
    }
    
    template<typename T>
    bool registerInstance(const std::string& name, T* instance) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (services_.find(name) != services_.end()) return false;
        
        ServiceEntry entry;
        entry.name = name;
        entry.typeName = typeid(T).name();
        entry.instance = ServiceHandle(instance, false);
        entry.singleton = true;
        entry.initialized = true;
        services_[name] = std::move(entry);
        return true;
    }
    
    template<typename T>
    bool registerShared(const std::string& name, std::shared_ptr<T> instance) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (services_.find(name) != services_.end()) return false;
        
        ServiceEntry entry;
        entry.name = name;
        entry.typeName = typeid(T).name();
        entry.instance = ServiceHandle(instance);
        entry.singleton = true;
        entry.initialized = true;
        services_[name] = std::move(entry);
        return true;
    }
    
    template<typename Interface, typename Impl>
    bool registerSingleton(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (services_.find(name) != services_.end()) return false;
        
        ServiceEntry entry;
        entry.name = name;
        entry.typeName = typeid(Interface).name();
        entry.singleton = true;
        entry.factory = ServiceFactory([]() -> std::shared_ptr<void> {
            return std::make_shared<Impl>();
        });
        services_[name] = std::move(entry);
        return true;
    }
    
    template<typename T>
    T* get(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) return nullptr;
        
        const ServiceEntry& entry = it->second;
        if (entry.singleton && entry.initialized) {
            return entry.instance.get<T>();
        }
        if (entry.factory.valid()) {
            auto inst = entry.factory.create();
            return inst ? static_cast<T*>(inst.get()) : nullptr;
        }
        return nullptr;
    }
    
    void* getRaw(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) return nullptr;
        
        const ServiceEntry& entry = it->second;
        if (entry.singleton && entry.initialized) {
            return entry.instance.getRaw();
        }
        if (entry.factory.valid()) {
            auto inst = entry.factory.create();
            return inst ? inst.get() : nullptr;
        }
        return nullptr;
    }
    
    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.find(name) != services_.end();
    }
    
    std::vector<std::string> listServices() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& p : services_) names.push_back(p.first);
        return names;
    }
    
    bool unregister(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.erase(name) > 0;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return services_.size();
    }
    
private:
    ServiceRegistry() = default;
    mutable std::mutex mutex_;
    std::map<std::string, ServiceEntry> services_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_SERVICE_REGISTRY_H
