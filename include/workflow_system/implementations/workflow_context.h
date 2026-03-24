/**
 * @file workflow_context.h
 * @brief 业务流程管理系统 - 工作流上下文实现
 *
 * 面向对象：
 * - 封装：管理工作流间的数据传递
 * - 依赖注入：接收资源管理器
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_CONTEXT_H
#define WORKFLOW_SYSTEM_WORKFLOW_CONTEXT_H

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include "interfaces/resource_manager.h"
#include "interfaces/workflow.h"
#include "core/any.h"
#include "core/logger.h"
#include "core/types.h"

namespace WorkflowSystem {

/**
 * @brief 工作流上下文实现
 *
 * 职责：在工作流之间传递数据和资源引用
 * 支持字符串和任意类型的对象存储
 */
class WorkflowContext : public IWorkflowContext {
public:
    explicit WorkflowContext(std::shared_ptr<IResourceManager> resourceManager)
        : resourceManager_(resourceManager) {}

    void set(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }

    std::string get(const std::string& key, const std::string& defaultValue) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : defaultValue;
    }

    bool has(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.find(key) != data_.end();
    }

    std::string getWorkingDir() const override {
        return get("working_dir", "");
    }

    void setWorkingDir(const std::string& path) override {
        // 注册为资源以便清理
        auto resource = resourceManager_->createDirectory(path);
        set("working_dir", path);
        set("working_dir_resource_id", resource->getId());
    }

    void cleanup() override {
        std::string resourceId;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            resourceId = data_["working_dir_resource_id"];
        }

        if (!resourceId.empty()) {
            resourceManager_->cleanup(resourceId);
        }
    }

    std::unordered_map<std::string, std::string> getAllData() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }

    // 对象存储实现（支持任意类型）
    void setObjectImpl(const std::string& key, const Any& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_[key] = value;
    }

    Any getObjectImpl(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(key);
        return (it != objects_.end()) ? it->second : Any();
    }

    std::unordered_map<std::string, Any> getAllObjects() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return objects_;
    }

    // 便捷方法：同时设置多个对象
    template<typename... Args>
    void setObjects(const std::string& key1, const Args&... args) {
        setObjectsImpl(key1, args...);
    }

    // 移除对象
    void removeObject(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.erase(key);
    }

private:
    // 递归实现可变参数模板
    template<typename T, typename... Args>
    void setObjectsImpl(const std::string& key, const T& value, const Args&... args) {
        setObjectImpl(key, Any(value));
        setObjectsImpl(args...);
    }

    // 递归终止条件
    void setObjectsImpl(const std::string& key, const Any& value) {
        setObjectImpl(key, value);
    }

    std::shared_ptr<IResourceManager> resourceManager_;
    std::unordered_map<std::string, std::string> data_;
    std::unordered_map<std::string, Any> objects_;
    mutable std::mutex mutex_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_CONTEXT_H
