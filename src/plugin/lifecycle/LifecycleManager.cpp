#include "workflow_system/plugin/lifecycle/LifecycleManager.hpp"
#include "workflow_system/core/logger.h"
#include <algorithm>

namespace WorkflowSystem { namespace Plugin {

void LifecycleManager::registerHook(LifecycleEvent event, LifecycleHook hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    hooks_[event].push_back(hook);
    LOG_INFO("注册生命周期钩子: " + getEventName(event));
}

void LifecycleManager::clearHooks() {
    std::lock_guard<std::mutex> lock(mutex_);
    hooks_.clear();
}

bool LifecycleManager::canTransition(PluginState from, PluginState to) const {
    // 初始化转换规则（第一次调用时）
    if (transitionRules_.empty()) {
        const_cast<LifecycleManager*>(this)->initializeTransitionRules();
    }
    
    // 相同状态总是允许
    if (from == to) {
        return true;
    }
    
    // 错误状态可以转换到卸载状态
    if (from == PluginState::ERROR && to == PluginState::UNLOADING) {
        return true;
    }
    
    // 任何状态都可以转换到错误状态
    if (to == PluginState::ERROR) {
        return true;
    }
    
    // 检查转换规则
    for (const auto& rule : transitionRules_) {
        if (rule.from == from && rule.to == to && rule.allowed) {
            return true;
        }
    }
    
    return false;
}

std::string LifecycleManager::getTransitionError(PluginState from, PluginState to) const {
    if (canTransition(from, to)) {
        return "";
    }
    
    return "不允许从 " + getStateName(from) + " 状态转换到 " + getStateName(to) + " 状态";
}

bool LifecycleManager::triggerEvent(const std::string& pluginId, LifecycleEvent event) {
    std::vector<LifecycleHook> hooksCopy;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = hooks_.find(event);
        if (it != hooks_.end()) {
            hooksCopy = it->second;
        }
    }
    
    LOG_INFO("触发生命周期事件: " + getEventName(event) + " 插件: " + pluginId);
    
    for (const auto& hook : hooksCopy) {
        try {
            if (!hook(pluginId, event)) {
                LOG_WARNING("生命周期钩子返回false: " + getEventName(event));
                return false;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("生命周期钩子异常: " + std::string(e.what()));
            return false;
        }
    }
    
    return true;
}

std::vector<PluginState> LifecycleManager::getAllowedTransitions(PluginState current) const {
    if (transitionRules_.empty()) {
        const_cast<LifecycleManager*>(this)->initializeTransitionRules();
    }
    
    std::vector<PluginState> allowed;
    
    for (const auto& rule : transitionRules_) {
        if (rule.from == current && rule.allowed) {
            allowed.push_back(rule.to);
        }
    }
    
    // 错误状态总是允许
    if (current != PluginState::ERROR) {
        allowed.push_back(PluginState::ERROR);
    }
    
    return allowed;
}

bool LifecycleManager::isRunningState(PluginState state) const {
    return state == PluginState::RUNNING || state == PluginState::STARTING;
}

bool LifecycleManager::isErrorState(PluginState state) const {
    return state == PluginState::ERROR;
}

bool LifecycleManager::isActiveState(PluginState state) const {
    return state == PluginState::LOADED ||
           state == PluginState::RESOLVED ||
           state == PluginState::INITIALIZED ||
           state == PluginState::STARTING ||
           state == PluginState::RUNNING ||
           state == PluginState::PAUSED;
}

std::string LifecycleManager::getStateName(PluginState state) {
    return stateToString(state);
}

std::string LifecycleManager::getEventName(LifecycleEvent event) {
    switch (event) {
        case LifecycleEvent::BEFORE_LOAD:      return "加载前";
        case LifecycleEvent::AFTER_LOAD:       return "加载后";
        case LifecycleEvent::BEFORE_INITIALIZE: return "初始化前";
        case LifecycleEvent::AFTER_INITIALIZE:  return "初始化后";
        case LifecycleEvent::BEFORE_START:     return "启动前";
        case LifecycleEvent::AFTER_START:      return "启动后";
        case LifecycleEvent::BEFORE_STOP:      return "停止前";
        case LifecycleEvent::AFTER_STOP:       return "停止后";
        case LifecycleEvent::BEFORE_UNLOAD:    return "卸载前";
        case LifecycleEvent::AFTER_UNLOAD:     return "卸载后";
        case LifecycleEvent::ON_ERROR:         return "发生错误";
        case LifecycleEvent::ON_STATE_CHANGE:  return "状态改变";
        default:                               return "未知事件";
    }
}

void LifecycleManager::initializeTransitionRules() {
    transitionRules_ = {
        // 从UNLOADED
        {PluginState::UNLOADED, PluginState::LOADING, true, "开始加载"},
        
        // 从LOADING
        {PluginState::LOADING, PluginState::LOADED, true, "加载完成"},
        
        // 从LOADED
        {PluginState::LOADED, PluginState::RESOLVING, true, "开始解析依赖"},
        {PluginState::LOADED, PluginState::DISABLED, true, "禁用插件"},
        
        // 从RESOLVING
        {PluginState::RESOLVING, PluginState::RESOLVED, true, "依赖解析完成"},
        
        // 从RESOLVED
        {PluginState::RESOLVED, PluginState::INITIALIZING, true, "开始初始化"},
        
        // 从INITIALIZING
        {PluginState::INITIALIZING, PluginState::INITIALIZED, true, "初始化完成"},
        
        // 从INITIALIZED
        {PluginState::INITIALIZED, PluginState::STARTING, true, "开始启动"},
        
        // 从STARTING
        {PluginState::STARTING, PluginState::RUNNING, true, "启动完成"},
        
        // 从RUNNING
        {PluginState::RUNNING, PluginState::STOPPING, true, "开始停止"},
        {PluginState::RUNNING, PluginState::PAUSED, true, "暂停插件"},
        
        // 从PAUSED
        {PluginState::PAUSED, PluginState::RUNNING, true, "恢复插件"},
        {PluginState::PAUSED, PluginState::STOPPING, true, "开始停止"},
        
        // 从STOPPING
        {PluginState::STOPPING, PluginState::STOPPED, true, "停止完成"},
        
        // 从STOPPED
        {PluginState::STOPPED, PluginState::STARTING, true, "重新启动"},
        {PluginState::STOPPED, PluginState::UNLOADING, true, "开始卸载"},
        
        // 从DISABLED
        {PluginState::DISABLED, PluginState::LOADING, true, "重新加载"},
        {PluginState::DISABLED, PluginState::UNLOADING, true, "开始卸载"},
        
        // 从ERROR
        {PluginState::ERROR, PluginState::UNLOADING, true, "错误后卸载"},
        
        // 从UNLOADING
        {PluginState::UNLOADING, PluginState::UNLOADED, true, "卸载完成"}
    };
}

} // namespace Plugin
} // namespace WorkflowSystem
