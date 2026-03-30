#ifndef PLUGIN_FRAMEWORK_DEPENDENCY_RESOLVER_HPP
#define PLUGIN_FRAMEWORK_DEPENDENCY_RESOLVER_HPP

#include "../core/PluginInfo.hpp"
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <functional>
#include <mutex>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 依赖解析结果
 */
struct ResolutionResult {
    bool success;                    // 是否成功
    std::vector<std::string> loadOrder;  // 加载顺序
    std::vector<std::string> errors;     // 错误信息
    std::vector<std::string> warnings;   // 警告信息
    
    static ResolutionResult ok(const std::vector<std::string>& order) {
        return {true, order, {}, {}};
    }
    
    static ResolutionResult error(const std::vector<std::string>& errs) {
        return {false, {}, errs, {}};
    }
};

/**
 * @brief 依赖冲突
 */
struct Conflict {
    std::string plugin1;        // 插件1 ID
    std::string plugin2;        // 插件2 ID
    std::string reason;         // 冲突原因
    
    std::string toString() const {
        return plugin1 + " <-> " + plugin2 + ": " + reason;
    }
};

/**
 * @brief 循环依赖
 */
struct Cycle {
    std::vector<std::string> plugins;  // 循环依赖链
    
    std::string toString() const {
        std::string result;
        for (size_t i = 0; i < plugins.size(); ++i) {
            if (i > 0) result += " -> ";
            result += plugins[i];
        }
        result += " -> " + plugins[0];  // 形成闭环
        return result;
    }
};

/**
 * @brief 依赖图节点
 */
struct DependencyNode {
    std::string pluginId;
    PluginSpec spec;
    std::set<std::string> dependencies;   // 此插件依赖的插件
    std::set<std::string> dependents;     // 依赖此插件的插件
    bool visited = false;
    bool inStack = false;                 // 用于循环检测
    
    DependencyNode() = default;
    explicit DependencyNode(const std::string& id) : pluginId(id) {}
};

/**
 * @brief 依赖解析器
 * 
 * 负责解析插件依赖关系，检测循环依赖和版本冲突
 */
class DependencyResolver {
public:
    DependencyResolver() = default;
    ~DependencyResolver() = default;
    
    /**
     * @brief 添加插件到解析器
     * @param spec 插件规范
     */
    void addPlugin(const PluginSpec& spec);
    
    /**
     * @brief 移除插件
     * @param pluginId 插件ID
     */
    void removePlugin(const std::string& pluginId);
    
    /**
     * @brief 清空所有插件
     */
    void clear();
    
    /**
     * @brief 解析所有依赖关系
     * @return 解析结果
     */
    ResolutionResult resolve();
    
    /**
     * @brief 解析单个插件的依赖
     * @param pluginId 插件ID
     * @return 解析结果
     */
    ResolutionResult resolvePlugin(const std::string& pluginId);
    
    /**
     * @brief 检测循环依赖
     * @return 检测到的所有循环
     */
    std::vector<Cycle> detectCycles() const;
    
    /**
     * @brief 检测版本冲突
     * @return 检测到的所有冲突
     */
    std::vector<Conflict> detectConflicts() const;
    
    /**
     * @brief 获取加载顺序（拓扑排序）
     * @return 插件ID列表，按加载顺序排列
     */
    std::vector<std::string> getLoadOrder() const;
    
    /**
     * @brief 获取卸载顺序（逆拓扑排序）
     * @return 插件ID列表，按卸载顺序排列
     */
    std::vector<std::string> getUnloadOrder() const;
    
    /**
     * @brief 获取插件的依赖列表
     * @param pluginId 插件ID
     * @return 依赖的插件ID列表
     */
    std::vector<std::string> getDependencies(const std::string& pluginId) const;
    
    /**
     * @brief 获取依赖此插件的插件列表
     * @param pluginId 插件ID
     * @return 依赖此插件的插件ID列表
     */
    std::vector<std::string> getDependents(const std::string& pluginId) const;
    
    /**
     * @brief 检查插件是否可以被卸载
     * @param pluginId 插件ID
     * @return 可以卸载返回true
     */
    bool canUnload(const std::string& pluginId) const;
    
    /**
     * @brief 检查依赖是否满足
     * @param pluginId 插件ID
     * @return 依赖满足返回true
     */
    bool checkDependencies(const std::string& pluginId) const;
    
    /**
     * @brief 获取依赖图
     * @return 依赖图（pluginId -> 依赖列表）
     */
    std::map<std::string, std::vector<std::string>> getDependencyGraph() const;
    
    /**
     * @brief 获取所有插件ID
     */
    std::vector<std::string> getAllPluginIds() const;
    
    /**
     * @brief 获取插件数量
     */
    size_t getPluginCount() const;
    
    /**
     * @brief 检查插件是否存在
     */
    bool hasPlugin(const std::string& pluginId) const;
    
private:
    /**
     * @brief 构建依赖图
     */
    void buildDependencyGraph();
    
    /**
     * @brief 深度优先搜索检测循环
     */
    bool detectCycleDFS(const std::string& pluginId,
                       std::vector<std::string>& path,
                       std::vector<Cycle>& cycles) const;
    
    /**
     * @brief 拓扑排序
     */
    std::vector<std::string> topologicalSort() const;
    
    /**
     * @brief 拓扑排序DFS
     */
    void topologicalSortDFS(const std::string& pluginId,
                           std::set<std::string>& visited,
                           std::vector<std::string>& result) const;
    
    /**
     * @brief 检查版本约束
     */
    bool checkVersionConstraint(const Version& version,
                               const VersionConstraint& constraint) const;
    
    std::map<std::string, std::shared_ptr<DependencyNode>> nodes_;
    mutable std::mutex mutex_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_DEPENDENCY_RESOLVER_HPP
