/**
 * @file system_facade.h
 * @brief 业务流程管理系统 - 系统门面
 *
 * 设计模式：
 * - 门面模式：为整个复杂系统提供简单接口
 * - 单例模式：确保系统门面全局唯一
 *
 * 面向对象：
 * - 封装：隐藏系统内部的复杂性
 * - 组合：组合管理器和资源管理器
 */

#ifndef WORKFLOW_SYSTEM_SYSTEM_FACADE_H
#define WORKFLOW_SYSTEM_SYSTEM_FACADE_H

#include <memory>
#include <mutex>
#include "interfaces/workflow_manager.h"
#include "implementations/workflow_manager.h"
#include "implementations/filesystem_resource_manager.h"
#include "core/logger.h"

namespace WorkflowSystem {

/**
 * @brief 系统门面
 *
 * 设计模式：门面模式
 * 为整个工作流管理系统提供统一的访问接口
 */
class SystemFacade {
public:
    static SystemFacade& getInstance() {
        static SystemFacade instance;
        return instance;
    }

    SystemFacade(const SystemFacade&) = delete;
    SystemFacade& operator=(const SystemFacade&) = delete;

    void initialize(const std::string& basePath = "/tmp/workflow_resources") {
        std::lock_guard<std::mutex> lock(mutex_);

        manager_ = std::make_shared<WorkflowManager>();
        resourceManager_ = std::dynamic_pointer_cast<FileSystemResourceManager>(
            manager_->getResourceManager()
        );

        LOG_INFO("[SystemFacade] System initialized with base path: " + basePath);
    }

    std::shared_ptr<IWorkflowManager> getManager() {
        return manager_;
    }

    std::shared_ptr<IResourceManager> getResourceManager() {
        return resourceManager_;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (manager_) {
            manager_->interrupt();
            manager_->cleanupAllResources();
        }

        LOG_INFO("[SystemFacade] System shutdown");
    }

private:
    SystemFacade() = default;

    std::shared_ptr<IWorkflowManager> manager_;
    std::shared_ptr<FileSystemResourceManager> resourceManager_;
    mutable std::mutex mutex_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_SYSTEM_FACADE_H
