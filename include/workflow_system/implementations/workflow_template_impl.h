/**
 * @file workflow_template_impl.h
 * @brief 业务流程管理系统 - 工作流模板实现
 *
 * 设计模式：
 * - 原型模式：通过模板创建具体工作流
 * - 建造者模式：逐步构建工作流模板
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_IMPL_H

#include <map>
#include <algorithm>
#include "workflow_system/interfaces/workflow_template.h"
#include "workflow_system/implementations/workflow_graph.h"
#include "workflow_system/implementations/workflows/base_workflow.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/types.h"

namespace WorkflowSystem {

/**
 * @brief 抽象工作流模板基类
 *
 * 提供模板参数验证的通用实现
 */
class AbstractWorkflowTemplate : public IWorkflowTemplate {
protected:
    std::string templateId_;
    std::string templateName_;
    std::string description_;
    std::string version_;
    std::vector<TemplateParameter> parameters_;

public:
    AbstractWorkflowTemplate(const std::string& id, const std::string& name,
                        const std::string& desc, const std::string& ver);

    virtual ~AbstractWorkflowTemplate();

    // ========== 模板元数据 ==========

    std::string getTemplateId() const override;

    std::string getTemplateName() const override;

    std::string getDescription() const override;

    std::string getVersion() const override;

    // ========== 参数管理 ==========

    std::vector<TemplateParameter> getParameters() const override;

    bool validateParameters(const std::map<std::string, Any>& params) const override;

protected:
    void addParameter(const TemplateParameter& param);

    Any getParameter(const std::map<std::string, Any>& params,
                   const std::string& name) const;
};

/**
 * @brief 工作流模板注册表实现
 */
class WorkflowTemplateRegistry : public IWorkflowTemplateRegistry {
private:
    std::map<std::string, std::shared_ptr<IWorkflowTemplate>> templates_;

public:
    WorkflowTemplateRegistry();

    virtual ~WorkflowTemplateRegistry();

    // ========== 模板注册 ==========

    void registerTemplate(const std::string& templateId,
                         std::shared_ptr<IWorkflowTemplate> templateDef) override;

    void unregisterTemplate(const std::string& templateId) override;

    bool hasTemplate(const std::string& templateId) const override;

    // ========== 模板查询 ==========

    std::vector<std::string> listTemplates() const override;

    std::shared_ptr<IWorkflowTemplate> getTemplate(
        const std::string& templateId) const override;

    // ========== 模板创建 ==========

    std::shared_ptr<IWorkflow> createWorkflowFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager
    ) const override;

    std::shared_ptr<IWorkflowGraph> createGraphFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params
    ) const override;
};

/**
 * @brief 模板参数构建器实现
 */
class TemplateParameterBuilder : public ITemplateParameterBuilder {
private:
    std::map<std::string, Any> parameters_;

public:
    TemplateParameterBuilder();

    virtual ~TemplateParameterBuilder();

    ITemplateParameterBuilder& addParameter(const TemplateParameter& param) override;

    ITemplateParameterBuilder& addParameter(
        const std::string& name,
        const std::string& type,
        Any defaultValue,
        bool required,
        const std::string& description
    ) override;

    std::map<std::string, Any> build() const override;

    void reset() override;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_IMPL_H
