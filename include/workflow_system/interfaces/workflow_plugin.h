/**
 * @file workflow_plugin.h
 * @brief 工作流插件系统接口 - 融合插件系统与工作流系统
 *
 * 功能：
 * - 将插件作为工作流节点执行
 * - 插件可以访问工作流上下文
 * - 支持插件的热加载和卸载
 * - 插件生命周期管理
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_PLUGIN_H
#define WORKFLOW_SYSTEM_WORKFLOW_PLUGIN_H

#include "workflow.h"
#include "workflow_graph.h"
#include "../../../plugins/include/plugin_interface.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace WorkflowSystem {

/**
 * @brief 工作流插件上下文
 *
 * 融合了工作流上下文和插件上下文
 */
class IWorkflowPluginContext : public Plugin::IPluginContext {
public:
    virtual ~IWorkflowPluginContext() = default;
    
    // ========== 工作流上下文功能 ==========
    
    /**
     * @brief 获取工作流上下文
     */
    virtual std::shared_ptr<IWorkflowContext> getWorkflowContext() const = 0;
    
    /**
     * @brief 获取当前节点ID
     */
    virtual std::string getCurrentNodeId() const = 0;
    
    /**
     * @brief 获取当前工作流名称
     */
    virtual std::string getWorkflowName() const = 0;
    
    // ========== 节点执行传递 ==========
    
    /**
     * @brief 设置输出数据（传递给下一个节点）
     */
    virtual void setOutput(const std::string& key, const std::string& value) = 0;
    
    /**
     * @brief 获取输入数据（来自上一个节点）
     */
    virtual std::string getInput(const std::string& key, const std::string& defaultValue = "") const = 0;
    
    /**
     * @brief 获取所有输入数据
     */
    virtual std::unordered_map<std::string, std::string> getAllInputs() const = 0;
    
    // ========== 执行控制 ==========
    
    /**
     * @brief 请求暂停工作流
     */
    virtual void requestPause() = 0;
    
    /**
     * @brief 请求停止工作流
     */
    virtual void requestStop() = 0;
    
    /**
     * @brief 请求跳过当前节点
     */
    virtual void requestSkip() = 0;
};

/**
 * @brief 工作流插件节点配置
 */
struct WorkflowPluginNodeConfig {
    std::string pluginName;           // 插件名称
    std::string pluginPath;           // 插件路径（.so文件）
    std::string action;               // 要执行的动作
    std::unordered_map<std::string, std::string> parameters;  // 插件参数
    bool autoLoad;                    // 是否自动加载插件
    int timeoutSeconds;               // 超时时间（秒）
    bool continueOnError;             // 出错时是否继续
    
    WorkflowPluginNodeConfig()
        : autoLoad(true)
        , timeoutSeconds(30)
        , continueOnError(false) {}
};

/**
 * @brief 工作流插件接口
 *
 * 工作流插件是特殊的插件，它可以直接作为工作流节点
 */
class IWorkflowPlugin : public Plugin::IPlugin {
public:
    virtual ~IWorkflowPlugin() = default;
    
    // ========== 工作流节点执行 ==========
    
    /**
     * @brief 执行插件（作为工作流节点）
     * @param context 工作流插件上下文
     * @param params 执行参数
     * @return 执行结果
     */
    virtual Plugin::PluginResult execute(
        IWorkflowPluginContext* context,
        const Plugin::PluginParams& params
    ) = 0;
    
    /**
     * @brief 验证执行参数
     * @param params 执行参数
     * @return 是否有效
     */
    virtual bool validateParams(const Plugin::PluginParams& params) const = 0;
    
    /**
     * @brief 获取插件支持的动作列表
     */
    virtual std::vector<std::string> getSupportedActions() const = 0;
    
    /**
     * @brief 获取动作的参数模板
     * @param action 动作名称
     * @return 参数描述（JSON格式）
     */
    virtual std::string getActionParamTemplate(const std::string& action) const = 0;
};

/**
 * @brief 工作流插件管理器接口
 *
 * 负责插件的加载、卸载、生命周期管理
 */
class IWorkflowPluginManager {
public:
    virtual ~IWorkflowPluginManager() = default;
    
    // ========== 插件加载 ==========
    
    /**
     * @brief 加载插件
     * @param pluginPath 插件路径（.so文件）
     * @return 插件名称，失败返回空字符串
     */
    virtual std::string loadPlugin(const std::string& pluginPath) = 0;
    
    /**
     * @brief 卸载插件
     * @param pluginName 插件名称
     * @return 是否成功
     */
    virtual bool unloadPlugin(const std::string& pluginName) = 0;
    
    /**
     * @brief 重载插件
     * @param pluginName 插件名称
     * @return 是否成功
     */
    virtual bool reloadPlugin(const std::string& pluginName) = 0;
    
    /**
     * @brief 从目录加载所有插件
     * @param directory 目录路径
     * @return 加载的插件数量
     */
    virtual int loadPluginsFromDirectory(const std::string& directory) = 0;
    
    // ========== 插件查询 ==========
    
    /**
     * @brief 获取插件
     * @param pluginName 插件名称
     * @return 插件指针，失败返回nullptr
     */
    virtual std::shared_ptr<IWorkflowPlugin> getPlugin(const std::string& pluginName) const = 0;
    
    /**
     * @brief 检查插件是否已加载
     * @param pluginName 插件名称
     * @return 是否已加载
     */
    virtual bool hasPlugin(const std::string& pluginName) const = 0;
    
    /**
     * @brief 获取所有已加载的插件名称
     */
    virtual std::vector<std::string> getLoadedPlugins() const = 0;
    
    /**
     * @brief 获取插件元数据
     * @param pluginName 插件名称
     * @return 插件元数据
     */
    virtual Plugin::PluginMetadata getPluginMetadata(const std::string& pluginName) const = 0;
    
    // ========== 插件执行 ==========
    
    /**
     * @brief 执行插件
     * @param pluginName 插件名称
     * @param context 工作流插件上下文
     * @param params 执行参数
     * @return 执行结果
     */
    virtual Plugin::PluginResult executePlugin(
        const std::string& pluginName,
        IWorkflowPluginContext* context,
        const Plugin::PluginParams& params
    ) = 0;
    
    // ========== 插件生命周期 ==========
    
    /**
     * @brief 初始化所有插件
     * @return 成功初始化的插件数量
     */
    virtual int initializeAll() = 0;
    
    /**
     * @brief 启动所有插件
     * @return 成功启动的插件数量
     */
    virtual int startAll() = 0;
    
    /**
     * @brief 停止所有插件
     * @return 成功停止的插件数量
     */
    virtual int stopAll() = 0;
    
    /**
     * @brief 卸载所有插件
     * @return 卸载的插件数量
     */
    virtual int unloadAll() = 0;
    
    // ========== 配置管理 ==========
    
    /**
     * @brief 设置插件目录
     */
    virtual void setPluginDirectory(const std::string& directory) = 0;
    
    /**
     * @brief 获取插件目录
     */
    virtual std::string getPluginDirectory() const = 0;
};

/**
 * @brief 创建工作流插件管理器
 */
extern "C" {
    /**
     * @brief 工厂函数：创建工作流插件管理器
     */
    std::shared_ptr<IWorkflowPluginManager> createWorkflowPluginManager();
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_PLUGIN_H
