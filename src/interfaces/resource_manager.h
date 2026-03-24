/**
 * @file resource_manager.h
 * @brief 业务流程管理系统 - 资源管理器接口
 *
 * 设计模式：
 * - 接口与实现分离：IResourceManager 接口
 * - 策略模式：不同的资源管理策略（文件系统、内存等）
 */

#ifndef WORKFLOW_SYSTEM_RESOURCE_MANAGER_H
#define WORKFLOW_SYSTEM_RESOURCE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "core/types.h"

namespace WorkflowSystem {

/**
 * @brief 资源实体
 *
 * 面向对象：封装资源的所有属性
 */
class Resource {
public:
    Resource(const std::string& id, ResourceType type, const std::string& path)
        : id_(id), type_(type), path_(path) {}

    // 访问器方法 - 封装原则
    const std::string& getId() const { return id_; }
    ResourceType getType() const { return type_; }
    const std::string& getPath() const { return path_; }

    // 修改器方法
    void setMetadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }

    std::string getMetadata(const std::string& key) const {
        auto it = metadata_.find(key);
        return (it != metadata_.end()) ? it->second : "";
    }

private:
    std::string id_;
    ResourceType type_;
    std::string path_;
    std::unordered_map<std::string, std::string> metadata_;
};

/**
 * @brief 资源管理器接口
 *
 * 设计模式：接口与实现分离
 * 定义资源管理的基本操作
 */
class IResourceManager {
public:
    virtual ~IResourceManager() = default;

    // 创建目录资源
    virtual std::shared_ptr<Resource> createDirectory(const std::string& path) = 0;

    // 创建临时目录
    virtual std::shared_ptr<Resource> createDirectory() = 0;

    // 获取资源
    virtual std::shared_ptr<Resource> getResource(const std::string& id) = 0;

    // 清理指定资源
    virtual bool cleanup(const std::string& id) = 0;

    // 清理所有资源
    virtual size_t cleanupAll() = 0;

    // 获取所有资源
    virtual std::vector<std::shared_ptr<Resource>> getAllResources() const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_RESOURCE_MANAGER_H
