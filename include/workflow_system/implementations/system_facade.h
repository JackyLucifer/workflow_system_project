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
#include <string>
#include "workflow_system/interfaces/workflow_manager.h"
#include "workflow_system/interfaces/workflow_orchestrator.h"
#include "workflow_system/interfaces/workflow_persistence.h"
#include "workflow_system/implementations/workflow_manager.h"
#include "workflow_system/implementations/workflow_orchestrator.h"
#include "workflow_system/implementations/filesystem_resource_manager.h"
#include "workflow_system/implementations/sqlite_workflow_persistence.h"
#include "workflow_system/core/logger.h"

namespace WorkflowSystem {

/**
 * @brief 系统配置选项
 */
struct SystemConfig {
    std::string basePath = "/tmp/workflow_resources";  // 资源基础路径
    std::string databasePath = "";                     // 数据库路径（空表示不启用持久化）
    bool enablePersistence = false;                    // 是否启用持久化
    int defaultTimeout = 30000;                        // 默认超时（毫秒）
};

/**
 * @brief 系统门面
 *
 * 设计模式：门面模式
 * 为整个工作流管理系统提供统一的访问接口
 *
 * 职责：
 * - 初始化系统组件
 * - 提供统一的工作流管理接口
 * - 管理系统生命周期
 * - 提供便捷的工作流操作方法
 */
class SystemFacade {
public:
    static SystemFacade& getInstance() {
        static SystemFacade instance;
        return instance;
    }

    SystemFacade(const SystemFacade&) = delete;
    SystemFacade& operator=(const SystemFacade&) = delete;

    /**
     * @brief 初始化系统
     *
     * @param config 系统配置
     *
     * 功能：初始化工作流管理器、资源管理器和持久化（如果启用）
     */
    void initialize(const SystemConfig& config = SystemConfig());

    /**
     * @brief 简化的初始化方法
     */
    void initialize(const std::string& basePath = "/tmp/workflow_resources");

    /**
     * @brief 启用持久化
     *
     * @param databasePath 数据库文件路径
     * @return 是否启用成功
     */
    bool enablePersistence(const std::string& databasePath);

    /**
     * @brief 禁用持久化
     */
    void disablePersistence();

    // ========== 工作流管理接口 ==========

    /**
     * @brief 注册工作流
     */
    bool registerWorkflow(const std::string& name, std::shared_ptr<IWorkflow> workflow);

    /**
     * @brief 启动工作流
     */
    bool startWorkflow(const std::string& name, std::shared_ptr<IWorkflowContext> context = nullptr);

    /**
     * @brief 切换工作流
     */
    bool switchWorkflow(const std::string& name, bool preserveContext = false);

    /**
     * @brief 中断当前工作流
     */
    void interrupt();

    /**
     * @brief 取消当前工作流
     */
    void cancel();

    // ========== 获取核心组件 ==========

    /**
     * @brief 获取工作流管理器
     */
    std::shared_ptr<IWorkflowManager> getManager();

    /**
     * @brief 获取资源管理器
     */
    std::shared_ptr<IResourceManager> getResourceManager();

    /**
     * @brief 获取工作流编排器
     */
    std::shared_ptr<IWorkflowOrchestrator> getOrchestrator();

    /**
     * @brief 获取持久化接口
     */
    IWorkflowPersistence* getPersistence();

    // ========== 系统状态查询 ==========

    /**
     * @brief 获取系统状态
     */
    std::string getStatus() const;

    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const;

    /**
     * @brief 检查持久化是否启用
     */
    bool isPersistenceEnabled() const;

    // ========== 便捷方法 ==========

    /**
     * @brief 发送消息到当前工作流
     */
    bool sendMessage(const std::string& topic, const std::string& content);

    /**
     * @brief 清理所有资源
     */
    void cleanupAllResources();

    /**
     * @brief 关闭系统
     */
    void shutdown();

private:
    SystemFacade() = default;

    std::shared_ptr<IWorkflowManager> manager_;
    std::shared_ptr<FileSystemResourceManager> resourceManager_;
    std::shared_ptr<WorkflowOrchestrator> orchestrator_;
    mutable std::mutex mutex_;

    bool initialized_ = false;
    bool persistenceEnabled_ = false;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_SYSTEM_FACADE_H
