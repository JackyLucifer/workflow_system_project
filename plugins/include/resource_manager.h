/**
 * @file resource_manager.h
 * @brief 资源管理器 - 插件资源生命周期管理
 */

#ifndef WORKFLOW_SYSTEM_RESOURCE_MANAGER_H
#define WORKFLOW_SYSTEM_RESOURCE_MANAGER_H

#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <functional>
#include <cstdint>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 资源类型
 */
enum class ResourceType {
    FILE,
    MEMORY,
    SOCKET,
    THREAD,
    HANDLE,
    CUSTOM
};

/**
 * @brief 资源信息
 */
struct Resource {
    uint64_t id;
    std::string name;
    ResourceType type;
    std::string owner;
    void* handle;
    size_t size;
    int64_t createTime;
    int64_t lastAccessTime;
    bool active;
    std::string description;
    
    Resource()
        : id(0)
        , type(ResourceType::CUSTOM)
        , handle(nullptr)
        , size(0)
        , createTime(0)
        , lastAccessTime(0)
        , active(false) {}
};

/**
 * @brief 资源清理器
 */
using ResourceCleaner = std::function<void(Resource&)>;

/**
 * @brief 资源管理器
 */
class ResourceManager {
public:
    static ResourceManager& getInstance() {
        static ResourceManager instance;
        return instance;
    }
    
    // ========== 资源注册 ==========
    
    /**
     * @brief 注册资源
     */
    uint64_t registerResource(const std::string& name,
                             ResourceType type,
                             void* handle,
                             const std::string& owner,
                             size_t size = 0,
                             const std::string& description = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint64_t id = nextResourceId_++;
        
        Resource res;
        res.id = id;
        res.name = name;
        res.type = type;
        res.handle = handle;
        res.owner = owner;
        res.size = size;
        res.createTime = getCurrentTime();
        res.lastAccessTime = res.createTime;
        res.active = true;
        res.description = description;
        
        resources_[id] = res;
        ownerResources_[owner].push_back(id);
        
        return id;
    }
    
    /**
     * @brief 注册文件资源
     */
    uint64_t registerFile(const std::string& filepath,
                         void* handle,
                         const std::string& owner,
                         size_t size = 0) {
        return registerResource(filepath, ResourceType::FILE, handle, owner, size, "File resource");
    }
    
    /**
     * @brief 注册内存资源
     */
    uint64_t registerMemory(void* ptr,
                           size_t size,
                           const std::string& owner,
                           const std::string& description = "Memory allocation") {
        return registerResource("memory_" + std::to_string(reinterpret_cast<uintptr_t>(ptr)),
                               ResourceType::MEMORY, ptr, owner, size, description);
    }
    
    /**
     * @brief 注册线程资源
     */
    uint64_t registerThread(void* threadHandle, const std::string& owner) {
        return registerResource("thread", ResourceType::THREAD, threadHandle, owner, 0, "Thread resource");
    }
    
    /**
     * @brief 注册自定义资源
     */
    uint64_t registerCustom(const std::string& name,
                           void* handle,
                           const std::string& owner,
                           const std::string& description = "") {
        return registerResource(name, ResourceType::CUSTOM, handle, owner, 0, description);
    }
    
    // ========== 资源释放 ==========
    
    /**
     * @brief 设置资源清理器
     */
    void setCleaner(uint64_t resourceId, ResourceCleaner cleaner) {
        std::lock_guard<std::mutex> lock(mutex_);
        cleaners_[resourceId] = cleaner;
    }
    
    /**
     * @brief 释放资源
     */
    bool releaseResource(uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = resources_.find(id);
        if (it == resources_.end() || !it->second.active) {
            return false;
        }
        
        Resource& res = it->second;
        res.active = false;
        
        auto cleanerIt = cleaners_.find(id);
        if (cleanerIt != cleaners_.end()) {
            cleanerIt->second(res);
            cleaners_.erase(cleanerIt);
        }
        
        std::string owner = res.owner;
        auto& ownerList = ownerResources_[owner];
        ownerList.erase(std::remove(ownerList.begin(), ownerList.end(), id), ownerList.end());
        
        resources_.erase(it);
        return true;
    }
    
    /**
     * @brief 释放插件所有资源
     */
    int releaseOwnerResources(const std::string& owner) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = ownerResources_.find(owner);
        if (it == ownerResources_.end()) {
            return 0;
        }
        
        std::vector<uint64_t> ids = it->second;
        int count = 0;
        
        for (uint64_t id : ids) {
            auto resIt = resources_.find(id);
            if (resIt != resources_.end() && resIt->second.active) {
                Resource& res = resIt->second;
                res.active = false;
                
                auto cleanerIt = cleaners_.find(id);
                if (cleanerIt != cleaners_.end()) {
                    cleanerIt->second(res);
                    cleaners_.erase(cleanerIt);
                }
                
                resources_.erase(resIt);
                count++;
            }
        }
        
        ownerResources_.erase(it);
        return count;
    }
    
    /**
     * @brief 释放所有资源
     */
    void releaseAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& pair : resources_) {
            if (pair.second.active) {
                auto cleanerIt = cleaners_.find(pair.first);
                if (cleanerIt != cleaners_.end()) {
                    cleanerIt->second(pair.second);
                }
            }
        }
        
        resources_.clear();
        ownerResources_.clear();
        cleaners_.clear();
    }
    
    // ========== 资源查询 ==========
    
    /**
     * @brief 获取资源信息
     */
    Resource getResource(uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(id);
        return (it != resources_.end()) ? it->second : Resource();
    }
    
    /**
     * @brief 获取插件的所有资源
     */
    std::vector<Resource> getOwnerResources(const std::string& owner) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Resource> result;
        auto it = ownerResources_.find(owner);
        if (it != ownerResources_.end()) {
            for (uint64_t id : it->second) {
                auto resIt = resources_.find(id);
                if (resIt != resources_.end()) {
                    result.push_back(resIt->second);
                }
            }
        }
        return result;
    }
    
    /**
     * @brief 获取所有活跃资源
     */
    std::vector<Resource> getActiveResources() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Resource> result;
        for (const auto& pair : resources_) {
            if (pair.second.active) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    /**
     * @brief 获取资源总数
     */
    size_t getResourceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return resources_.size();
    }
    
    /**
     * @brief 获取插件资源数量
     */
    size_t getOwnerResourceCount(const std::string& owner) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ownerResources_.find(owner);
        return (it != ownerResources_.end()) ? it->second.size() : 0;
    }
    
    /**
     * @brief 获取总内存使用量
     */
    size_t getTotalMemoryUsage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t total = 0;
        for (const auto& pair : resources_) {
            if (pair.second.active && pair.second.type == ResourceType::MEMORY) {
                total += pair.second.size;
            }
        }
        return total;
    }
    
    /**
     * @brief 更新资源访问时间
     */
    void touchResource(uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            it->second.lastAccessTime = getCurrentTime();
        }
    }

private:
    ResourceManager() : nextResourceId_(1) {}
    
    mutable std::mutex mutex_;
    uint64_t nextResourceId_;
    
    std::map<uint64_t, Resource> resources_;
    std::map<std::string, std::vector<uint64_t>> ownerResources_;
    std::map<uint64_t, ResourceCleaner> cleaners_;
    
    int64_t getCurrentTime() const {
        return std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    }
};

/**
 * @brief 资源守卫 - RAII 模式
 */
class ResourceGuard {
public:
    ResourceGuard(const std::string& owner)
        : owner_(owner), released_(false) {}
    
    ~ResourceGuard() {
        if (!released_) {
            release();
        }
    }
    
    uint64_t addFile(const std::string& filepath, void* handle, size_t size = 0) {
        uint64_t id = ResourceManager::getInstance().registerFile(filepath, handle, owner_, size);
        resources_.push_back(id);
        return id;
    }
    
    uint64_t addMemory(void* ptr, size_t size, const std::string& desc = "") {
        uint64_t id = ResourceManager::getInstance().registerMemory(ptr, size, owner_, desc);
        resources_.push_back(id);
        return id;
    }
    
    uint64_t addCustom(const std::string& name, void* handle, const std::string& desc = "") {
        uint64_t id = ResourceManager::getInstance().registerCustom(name, handle, owner_, desc);
        resources_.push_back(id);
        return id;
    }
    
    void setCleaner(uint64_t id, ResourceCleaner cleaner) {
        ResourceManager::getInstance().setCleaner(id, cleaner);
    }
    
    void release() {
        if (!released_) {
            ResourceManager::getInstance().releaseOwnerResources(owner_);
            resources_.clear();
            released_ = true;
        }
    }
    
    size_t count() const { return resources_.size(); }

private:
    std::string owner_;
    std::vector<uint64_t> resources_;
    bool released_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_RESOURCE_MANAGER_H
