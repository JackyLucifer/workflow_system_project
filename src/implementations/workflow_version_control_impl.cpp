/**
 * @file workflow_version_control_impl.cpp
 * @brief 业务流程管理系统 - 工作流版本控制实现
 */

#include "workflow_system/implementations/workflow_version_control_impl.h"

namespace WorkflowSystem {

// ========== WorkflowVersionControl 实现 ==========

WorkflowVersionControl::WorkflowVersionControl() {
    // 从持久化获取实例
    persistence_ = &SqliteWorkflowPersistence::getInstance();
}

WorkflowVersionControl::~WorkflowVersionControl() = default;

std::string WorkflowVersionControl::saveVersion(const std::string& workflowName,
                                                const std::string& definitionJson,
                                                const std::string& author,
                                                const std::string& description) {
    // 获取最新版本作为父版本
    auto latestVersion = getLatestVersion(workflowName);
    std::string parentVersionId = latestVersion.versionId.empty() ? "" : latestVersion.versionId;

    // 创建版本定义（包含工作流定义和版本信息）
    std::string versionId = generateVersionId(workflowName);
    int64_t now = TimeUtils::getCurrentMs();

    // 使用持久化的工作流定义表存储版本
    std::string versionDefJson = createVersionDefinitionJson(
        versionId, workflowName, definitionJson, author, description,
        parentVersionId, now, true);

    if (!persistence_->saveWorkflowDefinition(workflowName, versionDefJson)) {
        LOG_ERROR("[VersionControl] Failed to save version definition for " + workflowName);
        return "";
    }

    LOG_INFO("[VersionControl] Saved version " + versionId + " for workflow: " + workflowName);
    return versionId;
}

std::vector<WorkflowVersion> WorkflowVersionControl::getAllVersions(const std::string& workflowName) const {
    // 从持久化获取工作流定义
    std::string definitionJson = persistence_->getWorkflowDefinition(workflowName);

    if (definitionJson.empty()) {
        LOG_WARNING("[VersionControl] No definitions found for workflow: " + workflowName);
        return {};
    }

    // 解析包含所有版本信息的 JSON
    return parseVersionsFromJson(definitionJson);
}

WorkflowVersion WorkflowVersionControl::getLatestVersion(const std::string& workflowName) const {
    auto versions = getAllVersions(workflowName);
    if (versions.empty()) {
        return WorkflowVersion{};
    }
    return versions.front(); // 返回最新的（已按时间降序排列）
}

std::string WorkflowVersionControl::getVersionDefinition(const std::string& workflowName,
                                                         const std::string& versionId) const {
    auto versions = getAllVersions(workflowName);
    for (const auto& version : versions) {
        if (version.versionId == versionId) {
            return extractWorkflowDefinition(version.definitionJson);
        }
    }
    return "";
}

bool WorkflowVersionControl::rollbackToVersion(const std::string& workflowName,
                                               const std::string& versionId) {
    // 获取目标版本
    auto versions = getAllVersions(workflowName);
    WorkflowVersion targetVersion;
    bool found = false;
    for (const auto& version : versions) {
        if (version.versionId == versionId) {
            targetVersion = version;
            found = true;
            break;
        }
    }

    if (!found) {
        LOG_ERROR("[VersionControl] Version not found: " + versionId);
        return false;
    }

    // 提取工作流定义
    std::string workflowDef = extractWorkflowDefinition(targetVersion.definitionJson);

    // 创建新的版本定义
    std::string newVersionId = generateVersionId(workflowName);
    int64_t now = TimeUtils::getCurrentMs();
    std::string rollbackDefJson = createVersionDefinitionJson(
        newVersionId, workflowName, workflowDef, "System Rollback",
        "Rollback to version " + versionId,
        targetVersion.versionId, now, true);

    if (!persistence_->saveWorkflowDefinition(workflowName, rollbackDefJson)) {
        LOG_ERROR("[VersionControl] Failed to save rollback version");
        return false;
    }

    LOG_INFO("[VersionControl] Rolled back to version " + versionId);
    return true;
}

VersionDiff WorkflowVersionControl::compareVersions(const std::string& workflowName,
                                                    const std::string& versionFrom,
                                                    const std::string& versionTo) const {
    auto versions = getAllVersions(workflowName);

    WorkflowVersion vFrom, vTo;
    bool foundFrom = false, foundTo = false;

    for (const auto& version : versions) {
        if (!foundFrom && version.versionId == versionFrom) {
            vFrom = version;
            foundFrom = true;
        }
        if (!foundTo && version.versionId == versionTo) {
            vTo = version;
            foundTo = true;
        }
        if (foundFrom && foundTo) {
            break;
        }
    }

    if (!foundFrom || !foundTo) {
        return VersionDiff{};
    }

    // 简单的差异比较
    VersionDiff diff;
    diff.versionFrom = versionFrom;
    diff.versionTo = versionTo;

    // 提取工作流定义并比较（这里做简化处理）
    diff.summary = "Version comparison completed";

    return diff;
}

bool WorkflowVersionControl::setActiveVersion(const std::string& workflowName,
                                              const std::string& versionId) {
    // 获取所有版本
    auto versions = getAllVersions(workflowName);

    // 标记所有版本为非激活
    std::string allVersionsJson = createAllVersionsJson(versions, false);

    // 标记目标版本为激活
    for (auto& version : versions) {
        if (version.versionId == versionId) {
            version.isActive = true;
        } else {
            version.isActive = false;
        }
    }

    std::string updatedJson = createAllVersionsJson(versions, true);

    if (!persistence_->saveWorkflowDefinition(workflowName, updatedJson)) {
        LOG_ERROR("[VersionControl] Failed to update active version");
        return false;
    }

    LOG_INFO("[VersionControl] Set active version: " + versionId);
    return true;
}

bool WorkflowVersionControl::deleteVersion(const std::string& workflowName,
                                           const std::string& versionId) {
    auto versions = getAllVersions(workflowName);

    // 移除指定版本
    auto it = versions.begin();
    while (it != versions.end()) {
        if (it->versionId == versionId) {
            it = versions.erase(it);
            break;
        }
        ++it;
    }

    // 保存更新后的版本列表
    if (!versions.empty()) {
        std::string updatedJson = createAllVersionsJson(versions, false);
        if (!persistence_->saveWorkflowDefinition(workflowName, updatedJson)) {
            LOG_ERROR("[VersionControl] Failed to save after version deletion");
            return false;
        }
    } else {
        // 删除工作流定义
        persistence_->saveWorkflowDefinition(workflowName, "");
    }

    LOG_INFO("[VersionControl] Deleted version: " + versionId);
    return true;
}

int WorkflowVersionControl::deleteAllVersions(const std::string& workflowName) {
    // 删除工作流定义
    persistence_->saveWorkflowDefinition(workflowName, "");

    LOG_INFO("[VersionControl] Deleted all versions for workflow: " + workflowName);
    return 0; // 可以改为返回实际删除数量
}

std::vector<std::string> WorkflowVersionControl::getAllWorkflowNames() const {
    auto allDefs = persistence_->getAllWorkflowDefinitions();
    std::vector<std::string> names;
    for (const auto& pair : allDefs) {
        names.push_back(pair.first);
    }
    return names;
}

// ========== 私有辅助方法 ==========

std::string WorkflowVersionControl::generateVersionId(const std::string& workflowName) {
    int64_t now = TimeUtils::getCurrentMs();
    return "v" + std::to_string(now) + "_" + workflowName;
}

std::string WorkflowVersionControl::createVersionDefinitionJson(const std::string& versionId,
                                                                const std::string& workflowName,
                                                                const std::string& workflowDef,
                                                                const std::string& author,
                                                                const std::string& description,
                                                                const std::string& parentVersion,
                                                                int64_t createdAt,
                                                                bool isActive) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"versionId\": \"" << versionId << "\",\n";
    ss << "  \"workflowName\": \"" << workflowName << "\",\n";
    ss << "  \"definition\": " << workflowDef << ",\n";
    ss << "  \"metadata\": {\n";
    ss << "    \"author\": \"" << author << "\",\n";
    ss << "    \"description\": \"" << description << "\",\n";
    ss << "    \"createdAt\": " << createdAt << ",\n";
    ss << "    \"parentVersion\": \"" << parentVersion << "\",\n";
    ss << "    \"isActive\": " << (isActive ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string WorkflowVersionControl::createAllVersionsJson(const std::vector<WorkflowVersion>& versions,
                                                           bool preserveActiveFlag) {
    (void)preserveActiveFlag; // 保留参数以保持接口一致性

    std::stringstream ss;
    ss << "{\n";
    ss << "  \"versions\": [\n";

    bool first = true;
    for (const auto& version : versions) {
        if (!first) ss << ",\n";
        ss << "    {\n";
        ss << "      \"versionId\": \"" << version.versionId << "\",\n";
        ss << "      \"workflowName\": \"" << version.workflowName << "\",\n";
        ss << "      \"definition\": " << version.definitionJson << ",\n";
        ss << "      \"author\": \"" << version.author << "\",\n";
        ss << "      \"description\": \"" << version.description << "\",\n";
        ss << "      \"createdAt\": " << version.createdAt << ",\n";
        ss << "      \"parentVersion\": \"" << version.parentVersion << "\",\n";
        ss << "      \"isActive\": " << (version.isActive ? "true" : "false") << "\n";
        ss << "    }";
        first = false;
    }

    ss << "\n  ]\n";
    ss << "}";
    return ss.str();
}

std::vector<WorkflowVersion> WorkflowVersionControl::parseVersionsFromJson(const std::string& jsonStr) const {
    (void)jsonStr;
    std::vector<WorkflowVersion> versions;

    // 简化处理：查找版本信息
    // 实际应用中应该使用 JSON 解析库
    // 这里返回空列表，实际实现需要集成 JSON 解析器

    return versions;
}

std::string WorkflowVersionControl::extractWorkflowDefinition(const std::string& versionDefJson) const {
    (void)versionDefJson;
    // 简化处理：返回 definition 字段的内容
    // 实际应用中应该使用 JSON 解析器提取

    // 临时返回空字符串
    return "";
}

} // namespace WorkflowSystem
