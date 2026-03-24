/**
 * @file workflow_template.h
 * @brief 业务流程管理系统 - 工作流模板接口
 *
 * 设计模式：
 * - 原型模式：通过模板创建具体工作流
 * - 建造者模式：逐步构建工作流模板
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_H
#define WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "workflow.h"
#include "workflow_graph.h"
#include "resource_manager.h"
#include "core/types.h"

namespace WorkflowSystem {

/**
 * @brief 模板参数
 *
 * 用于模板实例化时的参数传递
 */
struct TemplateParameter {
    std::string name;           // 参数名称
    std::string type;           // 参数类型：string, int, bool, etc.
    Any defaultValue;            // 默认值
    bool required;               // 是否必填
    std::string description;      // 参数描述

    TemplateParameter()
        : required(false) {}
};

/**
 * @brief 工作流模板接口
 *
 * 定义可重用的工作流结构
 */
class IWorkflowTemplate {
public:
    virtual ~IWorkflowTemplate() = default;

    // ========== 模板元数据 ==========

    virtual std::string getTemplateId() const = 0;
    virtual std::string getTemplateName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getVersion() const = 0;

    // ========== 参数管理 ==========

    virtual std::vector<TemplateParameter> getParameters() const = 0;
    virtual bool validateParameters(const std::map<std::string, Any>& params) const = 0;

    // ========== 工作流创建 ==========

    virtual std::shared_ptr<IWorkflow> createWorkflow(
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager
    ) const = 0;

    virtual std::shared_ptr<IWorkflowGraph> createGraph(
        const std::map<std::string, Any>& params
    ) const = 0;
};

/**
 * @brief 工作流模板注册表接口
 *
 * 管理可用的工作流模板
 */
class IWorkflowTemplateRegistry {
public:
    virtual ~IWorkflowTemplateRegistry() = default;

    // ========== 模板注册 ==========

    virtual void registerTemplate(const std::string& templateId,
                               std::shared_ptr<IWorkflowTemplate> templateDef) = 0;
    virtual void unregisterTemplate(const std::string& templateId) = 0;
    virtual bool hasTemplate(const std::string& templateId) const = 0;

    // ========== 模板查询 ==========

    virtual std::vector<std::string> listTemplates() const = 0;
    virtual std::shared_ptr<IWorkflowTemplate> getTemplate(const std::string& templateId) const = 0;

    // ========== 模板创建 ==========

    virtual std::shared_ptr<IWorkflow> createWorkflowFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager
    ) const = 0;

    virtual std::shared_ptr<IWorkflowGraph> createGraphFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params
    ) const = 0;
};

/**
 * @brief 模板参数构建器接口
 *
 * 用于逐步构建模板参数
 */
class ITemplateParameterBuilder {
public:
    virtual ~ITemplateParameterBuilder() = default;

    virtual ITemplateParameterBuilder& addParameter(const TemplateParameter& param) = 0;
    virtual ITemplateParameterBuilder& addParameter(
        const std::string& name,
        const std::string& type,
        Any defaultValue,
        bool required = false,
        const std::string& description = ""
    ) = 0;
    virtual std::map<std::string, Any> build() const = 0;
    virtual void reset() = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_H
