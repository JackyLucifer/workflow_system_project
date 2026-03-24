/**
 * @file workflow_plugin_context.h
 * @brief 工作流插件上下文实现
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_PLUGIN_CONTEXT_H
#define WORKFLOW_SYSTEM_WORKFLOW_PLUGIN_CONTEXT_H

#include "workflow_system/interfaces/workflow_plugin.h"
#include "workflow_system/core/logger.h"
#include <unordered_map>
#include <functional>
#include <mutex>

namespace WorkflowSystem {

/**
 * @brief 工作流插件上下文实现
 */
class WorkflowPluginContext : public IWorkflowPluginContext {
public:
    WorkflowPluginContext(
        std::shared_ptr<IWorkflowContext> workflowContext,
        const std::string& nodeId,
        const std::string& workflowName
    ) : workflowContext_(workflowContext)
      , nodeId_(nodeId)
      , workflowName_(workflowName) {}
    
    virtual ~WorkflowPluginContext() = default;
    
    // ========== IPluginContext 实现 ==========
    
    void logInfo(const std::string& message) override {
        Logger::instance().info("[插件:" + nodeId_ + "] " + message);
    }
    
    void logWarning(const std::string& message) override {
        Logger::instance().warning("[插件:" + nodeId_ + "] " + message);
    }
    
    void logError(const std::string& message) override {
        Logger::instance().error("[插件:" + nodeId_ + "] " + message);
    }
    
    void logDebug(const std::string& message) override {
        Logger::instance().debug("[插件:" + nodeId_ + "] " + message);
    }
    
    std::string getConfig(const std::string& key, const std::string& defaultValue) const override {
        auto it = configs_.find(key);
        return (it != configs_.end()) ? it->second : defaultValue;
    }
    
    void setConfig(const std::string& key, const std::string& value) override {
        configs_[key] = value;
    }
    
    bool registerService(const std::string& serviceName, void* service) override {
        std::lock_guard<std::mutex> lock(mutex_);
        services_[serviceName] = service;
        return true;
    }
    
    void* getService(const std::string& serviceName) const override {
        auto it = services_.find(serviceName);
        return (it != services_.end()) ? it->second : nullptr;
    }
    
    bool hasService(const std::string& serviceName) const override {
        return services_.find(serviceName) != services_.end();
    }
    
    std::string getPluginPath() const override {
        return pluginPath_;
    }
    
    std::string getDataPath() const override {
        return dataPath_;
    }
    
    std::string getTempPath() const override {
        return tempPath_;
    }
    
    void emitEvent(const std::string& eventName, const std::string& eventData) override {
        auto it = eventCallbacks_.find(eventName);
        if (it != eventCallbacks_.end()) {
            for (auto& callback : it->second) {
                callback(eventData);
            }
        }
    }
    
    void subscribeEvent(const std::string& eventName, 
                       std::function<void(const std::string&)> callback) override {
        eventCallbacks_[eventName].push_back(callback);
    }
    
    // ========== IWorkflowPluginContext 实现 ==========
    
    std::shared_ptr<IWorkflowContext> getWorkflowContext() const override {
        return workflowContext_;
    }
    
    std::string getCurrentNodeId() const override {
        return nodeId_;
    }
    
    std::string getWorkflowName() const override {
        return workflowName_;
    }
    
    void setOutput(const std::string& key, const std::string& value) override {
        if (workflowContext_) {
            workflowContext_->set("output_" + key, value);
        }
    }
    
    std::string getInput(const std::string& key, const std::string& defaultValue) const override {
        if (workflowContext_) {
            return workflowContext_->get("input_" + key, defaultValue);
        }
        return defaultValue;
    }
    
    std::unordered_map<std::string, std::string> getAllInputs() const override {
        std::unordered_map<std::string, std::string> inputs;
        if (workflowContext_) {
            auto allData = workflowContext_->getAllData();
            for (const auto& pair : allData) {
                if (pair.first.substr(0, 6) == "input_") {
                    inputs[pair.first.substr(6)] = pair.second;
                }
            }
        }
        return inputs;
    }
    
    void requestPause() {
        pauseRequested_ = true;
    }
    
    void requestStop() {
        stopRequested_ = true;
    }
    
    void requestSkip() {
        skipRequested_ = true;
    }
    
    // ========== 辅助方法 ==========
    
    void setPluginPath(const std::string& path) {
        pluginPath_ = path;
    }
    
    void setDataPath(const std::string& path) {
        dataPath_ = path;
    }
    
    void setTempPath(const std::string& path) {
        tempPath_ = path;
    }
    
    bool isPauseRequested() const { return pauseRequested_; }
    bool isStopRequested() const { return stopRequested_; }
    bool isSkipRequested() const { return skipRequested_; }
    
    void resetFlags() {
        pauseRequested_ = false;
        stopRequested_ = false;
        skipRequested_ = false;
    }

private:
    std::shared_ptr<IWorkflowContext> workflowContext_;
    std::string nodeId_;
    std::string workflowName_;
    
    std::string pluginPath_;
    std::string dataPath_;
    std::string tempPath_;
    
    std::unordered_map<std::string, std::string> configs_;
    std::unordered_map<std::string, void*> services_;
    std::unordered_map<std::string, std::vector<std::function<void(const std::string&)>>> eventCallbacks_;
    
    mutable std::mutex mutex_;
    
    bool pauseRequested_ = false;
    bool stopRequested_ = false;
    bool skipRequested_ = false;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_PLUGIN_CONTEXT_H
