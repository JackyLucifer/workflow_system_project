/**
 * @file dependency_resolver.h
 * @brief 插件依赖解析器 - 拓扑排序解析依赖关系
 */

#ifndef WORKFLOW_SYSTEM_DEPENDENCY_RESOLVER_H
#define WORKFLOW_SYSTEM_DEPENDENCY_RESOLVER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <algorithm>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 依赖节点
 */
struct DependencyNode {
    std::string name;
    std::string version;
    std::vector<std::string> dependencies;
    int priority;
    bool optional;
    void* userData;
    
    DependencyNode() : priority(0), optional(false), userData(nullptr) {}
};

/**
 * @brief 依赖解析结果
 */
struct DependencyResult {
    bool success;
    std::vector<std::string> loadOrder;
    std::vector<std::string> missingDependencies;
    std::vector<std::string> circularDependencies;
    std::string errorMessage;
    
    DependencyResult() : success(true) {}
    
    static DependencyResult ok(const std::vector<std::string>& order) {
        DependencyResult r;
        r.success = true;
        r.loadOrder = order;
        return r;
    }
    
    static DependencyResult error(const std::string& msg) {
        DependencyResult r;
        r.success = false;
        r.errorMessage = msg;
        return r;
    }
};

/**
 * @brief 依赖解析器
 */
class DependencyResolver {
public:
    DependencyResolver() = default;
    
    /**
     * @brief 注册节点
     */
    void registerNode(const DependencyNode& node) {
        nodes_[node.name] = node;
    }
    
    /**
     * @brief 注册节点（简化版）
     */
    void registerNode(const std::string& name, 
                     const std::vector<std::string>& deps = {},
                     int priority = 0) {
        DependencyNode node;
        node.name = name;
        node.dependencies = deps;
        node.priority = priority;
        nodes_[name] = node;
    }
    
    /**
     * @brief 批量注册节点
     */
    void registerNodes(const std::vector<DependencyNode>& nodeList) {
        for (const auto& node : nodeList) {
            registerNode(node);
        }
    }
    
    /**
     * @brief 移除节点
     */
    bool removeNode(const std::string& name) {
        return nodes_.erase(name) > 0;
    }
    
    /**
     * @brief 清空所有节点
     */
    void clear() {
        nodes_.clear();
    }
    
    /**
     * @brief 解析依赖，返回加载顺序（使用 Kahn 算法，优先级仅在同层级节点间生效）
     */
    DependencyResult resolve() {
        DependencyResult result;
        
        if (nodes_.empty()) {
            return DependencyResult::ok({});
        }
        
        std::unordered_map<std::string, int> inDegree;
        std::unordered_map<std::string, std::vector<std::string>> graph;
        
        for (const auto& pair : nodes_) {
            inDegree[pair.first] = 0;
        }
        
        for (const auto& pair : nodes_) {
            for (const auto& dep : pair.second.dependencies) {
                if (nodes_.find(dep) != nodes_.end()) {
                    graph[dep].push_back(pair.first);
                    inDegree[pair.first]++;
                } else {
                    result.missingDependencies.push_back(pair.first + "->" + dep);
                }
            }
        }
        
        if (!result.missingDependencies.empty()) {
            result.success = false;
            result.errorMessage = "Missing dependencies detected";
            return result;
        }
        
        std::vector<std::string> order;
        std::vector<std::string> ready;
        
        for (const auto& pair : inDegree) {
            if (pair.second == 0) {
                ready.push_back(pair.first);
            }
        }
        
        while (!ready.empty()) {
            std::sort(ready.begin(), ready.end(), [this](const std::string& a, const std::string& b) {
                int prioA = nodes_.count(a) ? nodes_[a].priority : 0;
                int prioB = nodes_.count(b) ? nodes_[b].priority : 0;
                return prioA > prioB;
            });
            
            std::string current = ready.front();
            ready.erase(ready.begin());
            order.push_back(current);
            
            for (const auto& neighbor : graph[current]) {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0) {
                    ready.push_back(neighbor);
                }
            }
        }
        
        if (order.size() != nodes_.size()) {
            for (const auto& pair : nodes_) {
                if (std::find(order.begin(), order.end(), pair.first) == order.end()) {
                    result.circularDependencies.push_back(pair.first);
                }
            }
            result.success = false;
            result.errorMessage = "Circular dependency detected";
            return result;
        }
        
        result.success = true;
        result.loadOrder = order;
        return result;
    }
    
    /**
     * @brief 解析指定节点的依赖
     */
    DependencyResult resolveFor(const std::string& name) {
        DependencyResult result;
        
        if (nodes_.find(name) == nodes_.end()) {
            return DependencyResult::error("节点不存在: " + name);
        }
        
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> inStack;
        std::vector<std::string> order;
        
        if (!dfs(name, visited, inStack, order, result)) {
            result.success = false;
            return result;
        }
        
        std::reverse(order.begin(), order.end());
        result.success = true;
        result.loadOrder = order;
        return result;
    }
    
    /**
     * @brief 检查循环依赖
     */
    std::vector<std::string> detectCircularDependencies() {
        std::vector<std::string> circular;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> inStack;
        
        for (const auto& pair : nodes_) {
            std::vector<std::string> path;
            if (detectCycle(pair.first, visited, inStack, path)) {
                if (!path.empty()) {
                    circular.push_back(path.back());
                }
            }
        }
        
        return circular;
    }
    
    /**
     * @brief 获取缺失的依赖
     */
    std::vector<std::string> getMissingDependencies() {
        std::vector<std::string> missing;
        
        for (const auto& pair : nodes_) {
            for (const auto& dep : pair.second.dependencies) {
                if (nodes_.find(dep) == nodes_.end()) {
                    if (std::find(missing.begin(), missing.end(), dep) == missing.end()) {
                        missing.push_back(dep);
                    }
                }
            }
        }
        
        return missing;
    }
    
    /**
     * @brief 获取节点的所有依赖（递归）
     */
    std::set<std::string> getAllDependencies(const std::string& name) {
        std::set<std::string> allDeps;
        std::unordered_set<std::string> visited;
        
        collectDependencies(name, allDeps, visited);
        allDeps.erase(name);
        
        return allDeps;
    }
    
    /**
     * @brief 获取依赖某节点的所有节点
     */
    std::set<std::string> getDependents(const std::string& name) {
        std::set<std::string> dependents;
        
        for (const auto& pair : nodes_) {
            auto allDeps = getAllDependencies(pair.first);
            if (allDeps.count(name) > 0) {
                dependents.insert(pair.first);
            }
        }
        
        return dependents;
    }
    
    /**
     * @brief 检查节点是否存在
     */
    bool hasNode(const std::string& name) const {
        return nodes_.find(name) != nodes_.end();
    }
    
    /**
     * @brief 获取节点信息
     */
    DependencyNode getNode(const std::string& name) const {
        auto it = nodes_.find(name);
        return (it != nodes_.end()) ? it->second : DependencyNode();
    }
    
    /**
     * @brief 获取所有节点名称
     */
    std::vector<std::string> getAllNodeNames() const {
        std::vector<std::string> names;
        for (const auto& pair : nodes_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    size_t size() const { return nodes_.size(); }

private:
    std::unordered_map<std::string, DependencyNode> nodes_;
    
    bool dfs(const std::string& node,
             std::unordered_set<std::string>& visited,
             std::unordered_set<std::string>& inStack,
             std::vector<std::string>& order,
             DependencyResult& result) {
        
        if (inStack.count(node) > 0) {
            result.circularDependencies.push_back(node);
            return false;
        }
        
        if (visited.count(node) > 0) {
            return true;
        }
        
        auto it = nodes_.find(node);
        if (it == nodes_.end()) {
            result.missingDependencies.push_back(node);
            return false;
        }
        
        inStack.insert(node);
        
        for (const auto& dep : it->second.dependencies) {
            if (!dfs(dep, visited, inStack, order, result)) {
                return false;
            }
        }
        
        inStack.erase(node);
        visited.insert(node);
        order.push_back(node);
        
        return true;
    }
    
    bool detectCycle(const std::string& node,
                     std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& inStack,
                     std::vector<std::string>& path) {
        
        if (inStack.count(node) > 0) {
            return true;
        }
        
        if (visited.count(node) > 0) {
            return false;
        }
        
        auto it = nodes_.find(node);
        if (it == nodes_.end()) {
            return false;
        }
        
        visited.insert(node);
        inStack.insert(node);
        path.push_back(node);
        
        for (const auto& dep : it->second.dependencies) {
            if (detectCycle(dep, visited, inStack, path)) {
                return true;
            }
        }
        
        inStack.erase(node);
        path.pop_back();
        
        return false;
    }
    
    void collectDependencies(const std::string& name,
                            std::set<std::string>& allDeps,
                            std::unordered_set<std::string>& visited) {
        if (visited.count(name) > 0) {
            return;
        }
        
        visited.insert(name);
        allDeps.insert(name);
        
        auto it = nodes_.find(name);
        if (it != nodes_.end()) {
            for (const auto& dep : it->second.dependencies) {
                collectDependencies(dep, allDeps, visited);
            }
        }
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_DEPENDENCY_RESOLVER_H
