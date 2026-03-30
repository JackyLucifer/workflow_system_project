/**
 * @file system_facade.cpp
 * @brief 业务流程管理系统 - 系统门面实现
 */

#include "workflow_system/implementations/system_facade.h"

namespace WorkflowSystem {

void SystemFacade::initialize(const SystemConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 创建工作流管理器
    manager_ = std::make_shared<WorkflowManager>();
    resourceManager_ = std::dynamic_pointer_cast<FileSystemResourceManager>(
        manager_->getResourceManager()
    );

    // 设置默认超时
    manager_->setDefaultTimeout(config.defaultTimeout);

    // 启用持久化
    if (config.enablePersistence && !config.databasePath.empty()) {
        enablePersistence(config.databasePath);
    }

    // 创建编排器
    orchestrator_ = std::make_shared<WorkflowOrchestrator>();

    initialized_ = true;
    LOG_INFO("[SystemFacade] System initialized with base path: " + config.basePath);
}

void SystemFacade::initialize(const std::string& basePath) {
    SystemConfig config;
    config.basePath = basePath;
    initialize(config);
}

bool SystemFacade::enablePersistence(const std::string& databasePath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!manager_) {
        LOG_ERROR("[SystemFacade] Manager not initialized");
        return false;
    }

    // 通过 WorkflowManager 启用持久化
    auto* wfManager = dynamic_cast<WorkflowManager*>(manager_.get());
    if (!wfManager) {
        LOG_ERROR("[SystemFacade] Failed to cast to WorkflowManager");
        return false;
    }

    bool result = wfManager->enablePersistence(databasePath);
    if (result) {
        persistenceEnabled_ = true;
        LOG_INFO("[SystemFacade] Persistence enabled");
    }
    return result;
}

void SystemFacade::disablePersistence() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto* wfManager = dynamic_cast<WorkflowManager*>(manager_.get());
    if (wfManager) {
        wfManager->disablePersistence();
    }

    persistenceEnabled_ = false;
    LOG_INFO("[SystemFacade] Persistence disabled");
}

// ========== 工作流管理接口 ==========

bool SystemFacade::registerWorkflow(const std::string& name, std::shared_ptr<IWorkflow> workflow) {
    return manager_ ? manager_->registerWorkflow(name, workflow) : false;
}

bool SystemFacade::startWorkflow(const std::string& name, std::shared_ptr<IWorkflowContext> context) {
    return manager_ ? manager_->startWorkflow(name, context) : false;
}

bool SystemFacade::switchWorkflow(const std::string& name, bool preserveContext) {
    return manager_ ? manager_->switchWorkflow(name, preserveContext) : false;
}

void SystemFacade::interrupt() {
    if (manager_) manager_->interrupt();
}

void SystemFacade::cancel() {
    if (manager_) manager_->cancel();
}

// ========== 获取核心组件 ==========

std::shared_ptr<IWorkflowManager> SystemFacade::getManager() {
    return manager_;
}

std::shared_ptr<IResourceManager> SystemFacade::getResourceManager() {
    return resourceManager_;
}

std::shared_ptr<IWorkflowOrchestrator> SystemFacade::getOrchestrator() {
    return orchestrator_;
}

IWorkflowPersistence* SystemFacade::getPersistence() {
    if (manager_ && persistenceEnabled_) {
        auto* wfManager = dynamic_cast<WorkflowManager*>(manager_.get());
        if (wfManager && wfManager->isPersistenceEnabled()) {
            return &SqliteWorkflowPersistence::getInstance();
        }
    }
    return nullptr;
}

// ========== 系统状态查询 ==========

std::string SystemFacade::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string status = "{\n";
    status += "  \"initialized\": " + std::string(initialized_ ? "true" : "false") + ",\n";
    status += "  \"persistenceEnabled\": " + std::string(persistenceEnabled_ ? "true" : "false") + "\n";

    if (manager_) {
        status += ",\n" + manager_->getStatus();
    }

    status += "\n}";
    return status;
}

bool SystemFacade::isInitialized() const {
    return initialized_;
}

bool SystemFacade::isPersistenceEnabled() const {
    return persistenceEnabled_;
}

// ========== 便捷方法 ==========

bool SystemFacade::sendMessage(const std::string& topic, const std::string& content) {
    if (!manager_) return false;

    // 创建简单消息
    struct SimpleMessage : public IMessage {
        std::string type_;
        std::string topic_;
        std::string content_;

        SimpleMessage(const std::string& topic, const std::string& content)
            : type_("text"), topic_(topic), content_(content) {}

        std::string getType() const override { return type_; }
        std::string getTopic() const override { return topic_; }
        std::string getPayload() const override { return content_; }
    };

    SimpleMessage msg(topic, content);
    return manager_->handleMessage(msg);
}

void SystemFacade::cleanupAllResources() {
    if (manager_) {
        manager_->cleanupAllResources();
    }
}

void SystemFacade::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (manager_) {
        manager_->interrupt();
        manager_->cleanupAllResources();

        // 关闭持久化（如果启用）
        if (persistenceEnabled_) {
            auto* wfManager = dynamic_cast<WorkflowManager*>(manager_.get());
            if (wfManager) {
                wfManager->disablePersistence();
            }
        }
    }

    persistenceEnabled_ = false;

    // 清空所有指针，确保下次初始化时创建新实例
    manager_.reset();
    resourceManager_.reset();
    orchestrator_.reset();

    initialized_ = false;
    LOG_INFO("[SystemFacade] System shutdown");
}

} // namespace WorkflowSystem
