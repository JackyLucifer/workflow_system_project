#include "workflow_system/plugin/dependency/DependencyResolver.hpp"
#include "workflow_system/core/logger.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace WorkflowSystem { namespace Plugin {

void DependencyResolver::addPlugin(const PluginSpec& spec) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (spec.id.empty()) {
        LOG_ERROR("无法添加插件：插件ID为空");
        return;
    }
    
    auto node = std::make_shared<DependencyNode>(spec.id);
    node->spec = spec;
    
    // 添加依赖关系
    for (const auto& dep : spec.dependencies) {
        node->dependencies.insert(dep.pluginId);
    }
    
    nodes_[spec.id] = node;
    
    LOG_INFO("添加插件到依赖解析器: " + spec.id);
}

void DependencyResolver::removePlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return;
    }
    
    // 从其他插件的依赖列表中移除
    for (auto& pair : nodes_) {
        pair.second->dependencies.erase(pluginId);
        pair.second->dependents.erase(pluginId);
    }
    
    nodes_.erase(it);
    
    LOG_INFO("从依赖解析器移除插件: " + pluginId);
}

void DependencyResolver::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.clear();
}

ResolutionResult DependencyResolver::resolve() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // 构建依赖图
    buildDependencyGraph();
    
    // 检测循环依赖
    auto cycles = detectCycles();
    if (!cycles.empty()) {
        for (const auto& cycle : cycles) {
            errors.push_back("检测到循环依赖: " + cycle.toString());
        }
        return ResolutionResult::error(errors);
    }
    
    // 检测版本冲突
    auto conflicts = detectConflicts();
    for (const auto& conflict : conflicts) {
        warnings.push_back("版本冲突: " + conflict.toString());
    }
    
    // 检查所有依赖是否满足
    for (const auto& pair : nodes_) {
        const auto& node = pair.second;
        
        for (const auto& dep : node->spec.dependencies) {
            auto depIt = nodes_.find(dep.pluginId);
            
            if (depIt == nodes_.end()) {
                if (dep.required) {
                    errors.push_back("插件 " + node->pluginId + 
                                   " 缺少必需依赖: " + dep.pluginId);
                } else {
                    warnings.push_back("插件 " + node->pluginId + 
                                     " 缺少可选依赖: " + dep.pluginId);
                }
                continue;
            }
            
            // 检查版本约束
            if (!checkVersionConstraint(depIt->second->spec.version, 
                                       dep.versionConstraint)) {
                errors.push_back("插件 " + node->pluginId + 
                               " 的依赖 " + dep.pluginId + 
                               " 版本不满足: " + dep.versionConstraint.toString());
            }
        }
    }
    
    if (!errors.empty()) {
        return ResolutionResult::error(errors);
    }
    
    // 获取加载顺序
    auto loadOrder = topologicalSort();
    
    return {true, loadOrder, errors, warnings};
}

ResolutionResult DependencyResolver::resolvePlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return ResolutionResult::error({"插件不存在: " + pluginId});
    }
    
    std::vector<std::string> errors;
    std::vector<std::string> loadOrder;
    
    // 递归收集依赖
    std::set<std::string> visited;
    std::function<void(const std::string&)> collectDeps = 
        [&](const std::string& id) {
            if (visited.count(id) > 0) {
                return;
            }
            visited.insert(id);
            
            auto nodeIt = nodes_.find(id);
            if (nodeIt == nodes_.end()) {
                errors.push_back("依赖插件不存在: " + id);
                return;
            }
            
            // 先加载依赖
            for (const auto& dep : nodeIt->second->dependencies) {
                collectDeps(dep);
            }
            
            loadOrder.push_back(id);
        };
    
    collectDeps(pluginId);
    
    if (!errors.empty()) {
        return ResolutionResult::error(errors);
    }
    
    return {true, loadOrder, {}, {}};
}

std::vector<Cycle> DependencyResolver::detectCycles() const {
    std::vector<Cycle> cycles;
    
    // 重置访问状态
    for (auto& pair : nodes_) {
        pair.second->visited = false;
        pair.second->inStack = false;
    }
    
    // 对每个未访问的节点执行DFS
    for (const auto& pair : nodes_) {
        if (!pair.second->visited) {
            std::vector<std::string> path;
            detectCycleDFS(pair.first, path, cycles);
        }
    }
    
    return cycles;
}

bool DependencyResolver::detectCycleDFS(const std::string& pluginId,
                                       std::vector<std::string>& path,
                                       std::vector<Cycle>& cycles) const {
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return false;
    }
    
    auto& node = it->second;
    
    if (node->inStack) {
        // 找到循环
        Cycle cycle;
        bool found = false;
        for (const auto& id : path) {
            if (id == pluginId) {
                found = true;
            }
            if (found) {
                cycle.plugins.push_back(id);
            }
        }
        cycle.plugins.push_back(pluginId);
        cycles.push_back(cycle);
        return true;
    }
    
    if (node->visited) {
        return false;
    }
    
    node->visited = true;
    node->inStack = true;
    path.push_back(pluginId);
    
    bool hasCycle = false;
    for (const auto& dep : node->dependencies) {
        if (detectCycleDFS(dep, path, cycles)) {
            hasCycle = true;
        }
    }
    
    path.pop_back();
    node->inStack = false;
    
    return hasCycle;
}

std::vector<Conflict> DependencyResolver::detectConflicts() const {
    std::vector<Conflict> conflicts;
    
    // 检查冲突插件列表
    for (const auto& pair : nodes_) {
        const auto& node = pair.second;
        
        for (const auto& conflictId : node->spec.conflicts) {
            auto conflictIt = nodes_.find(conflictId);
            if (conflictIt != nodes_.end()) {
                Conflict conflict;
                conflict.plugin1 = node->pluginId;
                conflict.plugin2 = conflictId;
                conflict.reason = "插件声明冲突";
                conflicts.push_back(conflict);
            }
        }
    }
    
    return conflicts;
}

std::vector<std::string> DependencyResolver::getLoadOrder() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return topologicalSort();
}

std::vector<std::string> DependencyResolver::getUnloadOrder() const {
    auto loadOrder = getLoadOrder();
    std::reverse(loadOrder.begin(), loadOrder.end());
    return loadOrder;
}

std::vector<std::string> DependencyResolver::getDependencies(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second->dependencies.begin(),
                                   it->second->dependencies.end());
}

std::vector<std::string> DependencyResolver::getDependents(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second->dependents.begin(),
                                   it->second->dependents.end());
}

bool DependencyResolver::canUnload(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return true;
    }
    
    // 如果有其他插件依赖此插件，则不能卸载
    return it->second->dependents.empty();
}

bool DependencyResolver::checkDependencies(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return false;
    }
    
    const auto& node = it->second;
    
    for (const auto& dep : node->spec.dependencies) {
        if (dep.required && nodes_.find(dep.pluginId) == nodes_.end()) {
            return false;
        }
    }
    
    return true;
}

std::map<std::string, std::vector<std::string>> 
DependencyResolver::getDependencyGraph() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, std::vector<std::string>> graph;
    
    for (const auto& pair : nodes_) {
        std::vector<std::string> deps(pair.second->dependencies.begin(),
                                      pair.second->dependencies.end());
        graph[pair.first] = deps;
    }
    
    return graph;
}

std::vector<std::string> DependencyResolver::getAllPluginIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(nodes_.size());
    
    for (const auto& pair : nodes_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

size_t DependencyResolver::getPluginCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

bool DependencyResolver::hasPlugin(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.find(pluginId) != nodes_.end();
}

void DependencyResolver::buildDependencyGraph() {
    // 清除旧的依赖关系
    for (auto& pair : nodes_) {
        pair.second->dependents.clear();
    }
    
    // 建立反向依赖关系
    for (auto& pair : nodes_) {
        auto& node = pair.second;
        
        for (const auto& depId : node->dependencies) {
            auto depIt = nodes_.find(depId);
            if (depIt != nodes_.end()) {
                depIt->second->dependents.insert(node->pluginId);
            }
        }
    }
}

std::vector<std::string> DependencyResolver::topologicalSort() const {
    std::vector<std::string> result;
    std::set<std::string> visited;
    
    for (const auto& pair : nodes_) {
        if (visited.find(pair.first) == visited.end()) {
            topologicalSortDFS(pair.first, visited, result);
        }
    }
    
    return result;
}

void DependencyResolver::topologicalSortDFS(const std::string& pluginId,
                                           std::set<std::string>& visited,
                                           std::vector<std::string>& result) const {
    visited.insert(pluginId);
    
    auto it = nodes_.find(pluginId);
    if (it == nodes_.end()) {
        return;
    }
    
    // 先处理依赖
    for (const auto& depId : it->second->dependencies) {
        if (visited.find(depId) == visited.end()) {
            topologicalSortDFS(depId, visited, result);
        }
    }
    
    result.push_back(pluginId);
}

bool DependencyResolver::checkVersionConstraint(const Version& version,
                                               const VersionConstraint& constraint) const {
    return constraint.satisfies(version);
}

} // namespace Plugin
} // namespace WorkflowSystem
