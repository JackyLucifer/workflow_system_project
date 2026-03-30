#ifndef PLUGIN_FRAMEWORK_LIFECYCLE_MANAGER_HPP
#define PLUGIN_FRAMEWORK_LIFECYCLE_MANAGER_HPP

#include "../core/PluginInfo.hpp"
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 生命周期事件类型
 */
enum class LifecycleEvent {
    BEFORE_LOAD,        // 加载前
    AFTER_LOAD,         // 加载后
    BEFORE_INITIALIZE,  // 初始化前
    AFTER_INITIALIZE,   // 初始化后
    BEFORE_START,       // 启动前
    AFTER_START,        // 启动后
    BEFORE_STOP,        // 停止前
    AFTER_STOP,         // 停止后
    BEFORE_UNLOAD,      // 卸载前
    AFTER_UNLOAD,       // 卸载后
    ON_ERROR,           // 发生错误
    ON_STATE_CHANGE     // 状态改变
};

/**
 * @brief 生命周期钩子回调
 */
using LifecycleHook = std::function<bool(const std::string& pluginId, LifecycleEvent event)>;

/**
 * @brief 状态转换规则
 */
struct StateTransition {
    PluginState from;
    PluginState to;
    bool allowed;
    std::string reason;
};

/**
 * @brief 生命周期管理器
 * 
 * 管理插件的状态转换和生命周期事件
 */
class LifecycleManager {
public:
    LifecycleManager() = default;
    ~LifecycleManager() = default;
    
    /**
     * @brief 注册生命周期钩子
     * @param event 事件类型
     * @param hook 钩子函数
     */
    void registerHook(LifecycleEvent event, LifecycleHook hook);
    
    /**
     * @brief 清除所有钩子
     */
    void clearHooks();
    
    /**
     * @brief 验证状态转换
     * @param from 当前状态
     * @param to 目标状态
     * @return 允许转换返回true
     */
    bool canTransition(PluginState from, PluginState to) const;
    
    /**
     * @brief 获取状态转换的错误原因
     * @param from 当前状态
     * @param to 目标状态
     * @return 错误原因
     */
    std::string getTransitionError(PluginState from, PluginState to) const;
    
    /**
     * @brief 触发生命周期事件
     * @param pluginId 插件ID
     * @param event 事件类型
     * @return 所有钩子都返回true则返回true
     */
    bool triggerEvent(const std::string& pluginId, LifecycleEvent event);
    
    /**
     * @brief 获取下一个允许的状态列表
     * @param current 当前状态
     * @return 允许转换到的状态列表
     */
    std::vector<PluginState> getAllowedTransitions(PluginState current) const;
    
    /**
     * @brief 检查状态是否为运行状态
     */
    bool isRunningState(PluginState state) const;
    
    /**
     * @brief 检查状态是否为错误状态
     */
    bool isErrorState(PluginState state) const;
    
    /**
     * @brief 检查状态是否为活跃状态（已加载或运行中）
     */
    bool isActiveState(PluginState state) const;
    
    /**
     * @brief 获取状态的友好名称
     */
    static std::string getStateName(PluginState state);
    
    /**
     * @brief 获取事件的友好名称
     */
    static std::string getEventName(LifecycleEvent event);
    
private:
    /**
     * @brief 初始化状态转换规则
     */
    void initializeTransitionRules();
    
    std::map<LifecycleEvent, std::vector<LifecycleHook>> hooks_;
    std::vector<StateTransition> transitionRules_;
    mutable std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_LIFECYCLE_MANAGER_HPP
