/**
 * @file workflow_version_control.h
 * @brief 业务流程管理系统 - 工作流版本控制接口
 *
 * 设计模式：
 * - 版本控制：管理工作流定义的版本历史
 * - 回滚机制：支持回滚到之前版本
 * - 差异对比：比较不同版本的差异
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_VERSION_CONTROL_H
#define WORKFLOW_SYSTEM_WORKFLOW_VERSION_CONTROL_H

#include <string>
#include <vector>
#include <memory>

namespace WorkflowSystem {

/**
 * @brief 工作流版本信息
 */
struct WorkflowVersion {
    std::string versionId;      // 版本ID (格式：v1, v2, ...)
    std::string workflowName;  // 工作流名称
    std::string author;         // 作者
    std::string description;      // 版本描述
    int64_t createdAt;       // 创建时间（毫秒时间戳）
    std::string definitionJson;  // 工作流定义（JSON）
    std::string parentVersion; // 父版本ID（用于分支/回滚）
    bool isActive;           // 是否为当前激活版本
};

/**
 * @brief 版本差异信息
 */
struct VersionDiff {
    std::string versionFrom;     // 源版本
    std::string versionTo;       // 目标版本
    std::vector<std::string> changedNodes;  // 修改的节点列表
    std::vector<std::string> changedEdges;  // 修改的边列表
    std::string summary;          // 差异摘要
};

/**
 * @brief 工作流版本控制接口
 *
 * 功能：
 * - 保存工作流版本
 * - 获取版本历史
 * - 回滚到指定版本
 * - 比较版本差异
 * - 创建版本分支
 */
class IWorkflowVersionControl {
public:
    virtual ~IWorkflowVersionControl() = default;

    /**
     * @brief 保存工作流的新版本
     * @param workflowName 工作流名称
     * @param definitionJson 工作流定义（JSON格式）
     * @param author 作者
     * @param description 版本描述
     * @return 新创建的版本ID
     */
    virtual std::string saveVersion(const std::string& workflowName,
                                       const std::string& definitionJson,
                                       const std::string& author = "",
                                       const std::string& description = "") = 0;

    /**
     * @brief 获取工作流的所有版本
     * @param workflowName 工作流名称
     * @return 版本列表，按创建时间降序排列
     */
    virtual std::vector<WorkflowVersion> getAllVersions(const std::string& workflowName) const = 0;

    /**
     * @brief 获取指定工作流的最新版本
     * @param workflowName 工作流名称
     * @return 最新版本信息
     */
    virtual WorkflowVersion getLatestVersion(const std::string& workflowName) const = 0;

    /**
     * @brief 获取指定版本的工作流定义
     * @param workflowName 工作流名称
     * @param versionId 版本ID
     * @return 工作流定义（JSON），如果版本不存在返回空字符串
     */
    virtual std::string getVersionDefinition(const std::string& workflowName,
                                            const std::string& versionId) const = 0;

    /**
     * @brief 回滚工作流到指定版本
     * @param workflowName 工作流名称
     * @param versionId 要回滚到的版本ID
     * @return 是否回滚成功
     */
    virtual bool rollbackToVersion(const std::string& workflowName,
                                   const std::string& versionId) = 0;

    /**
     * @brief 比较两个版本之间的差异
     * @param workflowName 工作流名称
     * @param versionFrom 源版本ID
     * @param versionTo 目标版本ID
     * @return 版本差异信息
     */
    virtual VersionDiff compareVersions(const std::string& workflowName,
                                     const std::string& versionFrom,
                                     const std::string& versionTo) const = 0;

    /**
     * @brief 标记指定版本为激活版本
     * @param workflowName 工作流名称
     * @param versionId 版本ID
     * @return 是否操作成功
     */
    virtual bool setActiveVersion(const std::string& workflowName,
                                     const std::string& versionId) = 0;

    /**
     * @brief 删除指定版本
     * @param workflowName 工作流名称
     * @param versionId 版本ID
     * @return 是否删除成功
     */
    virtual bool deleteVersion(const std::string& workflowName,
                                 const std::string& versionId) = 0;

    /**
     * @brief 删除工作流的所有版本
     * @param workflowName 工作流名称
     * @return 删除的版本数量
     */
    virtual int deleteAllVersions(const std::string& workflowName) = 0;

    /**
     * @brief 获取所有工作流名称
     * @return 工作流名称列表
     */
    virtual std::vector<std::string> getAllWorkflowNames() const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_VERSION_CONTROL_H
