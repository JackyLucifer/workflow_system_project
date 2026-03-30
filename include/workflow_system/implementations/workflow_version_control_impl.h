/**
 * @file workflow_version_control_impl.h
 * @brief 业务流程管理系统 - 工作流版本控制实现
 *
 * 设计模式：
 * - 适配器模式：使用持久化接口存储版本
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_VERSION_CONTROL_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_VERSION_CONTROL_IMPL_H

#include <map>
#include <mutex>
#include <algorithm>
#include <sstream>
#include "workflow_system/interfaces/workflow_version_control.h"
#include "workflow_system/interfaces/workflow_persistence.h"
#include "workflow_system/implementations/sqlite_workflow_persistence.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"

namespace WorkflowSystem {

/**
 * @brief 工作流版本控制实现
 */
class WorkflowVersionControl : public IWorkflowVersionControl {
public:
    WorkflowVersionControl();

    virtual ~WorkflowVersionControl() override;

    std::string saveVersion(const std::string& workflowName,
                           const std::string& definitionJson,
                           const std::string& author,
                           const std::string& description) override;

    std::vector<WorkflowVersion> getAllVersions(const std::string& workflowName) const override;

    WorkflowVersion getLatestVersion(const std::string& workflowName) const override;

    std::string getVersionDefinition(const std::string& workflowName,
                                    const std::string& versionId) const override;

    bool rollbackToVersion(const std::string& workflowName,
                          const std::string& versionId) override;

    VersionDiff compareVersions(const std::string& workflowName,
                               const std::string& versionFrom,
                               const std::string& versionTo) const override;

    bool setActiveVersion(const std::string& workflowName,
                         const std::string& versionId) override;

    bool deleteVersion(const std::string& workflowName,
                      const std::string& versionId) override;

    int deleteAllVersions(const std::string& workflowName) override;

    std::vector<std::string> getAllWorkflowNames() const override;

private:
    IWorkflowPersistence* persistence_;

    /**
     * @brief 生成版本ID
     */
    std::string generateVersionId(const std::string& workflowName);

    /**
     * @brief 创建版本定义 JSON
     */
    std::string createVersionDefinitionJson(const std::string& versionId,
                                           const std::string& workflowName,
                                           const std::string& workflowDef,
                                           const std::string& author,
                                           const std::string& description,
                                           const std::string& parentVersion,
                                           int64_t createdAt,
                                           bool isActive);

    /**
     * @brief 创建包含所有版本的 JSON
     */
    std::string createAllVersionsJson(const std::vector<WorkflowVersion>& versions,
                                     bool preserveActiveFlag);

    /**
     * @brief 从 JSON 解析版本列表
     */
    std::vector<WorkflowVersion> parseVersionsFromJson(const std::string& jsonStr) const;

    /**
     * @brief 从版本定义中提取工作流定义
     */
    std::string extractWorkflowDefinition(const std::string& versionDefJson) const;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_VERSION_CONTROL_IMPL_H
