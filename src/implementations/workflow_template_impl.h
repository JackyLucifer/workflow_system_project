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
#include "interfaces/workflow_template.h"
#include "implementations/workflow_graph.h"
#include "implementations/workflows/base_workflow.h"
#include "core/logger.h"
#include "core/types.h"

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
                        const std::string& desc, const std::string& ver)
        : templateId_(id), templateName_(name),
          description_(desc), version_(ver) {}

    virtual ~AbstractWorkflowTemplate() = default;

    // ========== 模板元数据 ==========

    std::string getTemplateId() const override {
        return templateId_;
    }

    std::string getTemplateName() const override {
        return templateName_;
    }

    std::string getDescription() const override {
        return description_;
    }

    std::string getVersion() const override {
        return version_;
    }

    // ========== 参数管理 ==========

    std::vector<TemplateParameter> getParameters() const override {
        return parameters_;
    }

    bool validateParameters(const std::map<std::string, Any>& params) const override {
        // 检查必填参数
        for (const auto& param : parameters_) {
            if (param.required && params.find(param.name) == params.end()) {
                LOG_WARNING("[AbstractWorkflowTemplate] Required parameter '" +
                         param.name + "' is missing for template '" + templateId_ + "'");
                return false;
            }

            // 验证参数类型
            auto it = params.find(param.name);
            if (it != params.end()) {
                // 使用 typeid 比较进行类型验证
                if (param.type == "int" && typeid(int) != it->second.type()) {
                    LOG_WARNING("[AbstractWorkflowTemplate] Parameter '" + param.name +
                                 "' should be int for template '" + templateId_ + "'");
                    return false;
                }
                if (param.type == "string" && typeid(std::string) != it->second.type()) {
                    LOG_WARNING("[AbstractWorkflowTemplate] Parameter '" + param.name +
                                 "' should be string for template '" + templateId_ + "'");
                    return false;
                }
                if (param.type == "bool" && typeid(bool) != it->second.type()) {
                    LOG_WARNING("[AbstractWorkflowTemplate] Parameter '" + param.name +
                                 "' should be bool for template '" + templateId_ + "'");
                    return false;
                }
            }
        }

        return true;
    }

protected:
    void addParameter(const TemplateParameter& param) {
        parameters_.push_back(param);
    }

    Any getParameter(const std::map<std::string, Any>& params,
                   const std::string& name) const {
        auto it = params.find(name);
        if (it != params.end()) {
            return it->second;
        }

        // 查找默认值
        for (const auto& param : parameters_) {
            if (param.name == name) {
                return param.defaultValue;
            }
        }

        return Any();
    }
};

/**
 * @brief 工作流模板注册表实现
 */
class WorkflowTemplateRegistry : public IWorkflowTemplateRegistry {
private:
    std::map<std::string, std::shared_ptr<IWorkflowTemplate>> templates_;

public:
    WorkflowTemplateRegistry() {
        LOG_INFO("[WorkflowTemplateRegistry] Created");
    }

    virtual ~WorkflowTemplateRegistry() {
        templates_.clear();
        LOG_INFO("[WorkflowTemplateRegistry] Destroyed");
    }

    // ========== 模板注册 ==========

    void registerTemplate(const std::string& templateId,
                               std::shared_ptr<IWorkflowTemplate> templateDef) override {
        templates_[templateId] = templateDef;
        LOG_INFO("[WorkflowTemplateRegistry] Registered template: " + templateId +
                 " (" + templateDef->getTemplateName() + ")");
    }

    void unregisterTemplate(const std::string& templateId) override {
        templates_.erase(templateId);
        LOG_INFO("[WorkflowTemplateRegistry] Unregistered template: " + templateId);
    }

    bool hasTemplate(const std::string& templateId) const override {
        return templates_.find(templateId) != templates_.end();
    }

    // ========== 模板查询 ==========

    std::vector<std::string> listTemplates() const override {
        std::vector<std::string> result;
        for (const auto& pair : templates_) {
            result.push_back(pair.first);
        }
        return result;
    }

    std::shared_ptr<IWorkflowTemplate> getTemplate(
        const std::string& templateId) const override {
        auto it = templates_.find(templateId);
        if (it != templates_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // ========== 模板创建 ==========

    std::shared_ptr<IWorkflow> createWorkflowFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager
    ) const override {
        auto templateDef = getTemplate(templateId);
        if (!templateDef) {
            LOG_ERROR("[WorkflowTemplateRegistry] Template not found: " + templateId);
            return nullptr;
        }

        // 验证参数
        if (!templateDef->validateParameters(params)) {
            LOG_ERROR("[WorkflowTemplateRegistry] Parameter validation failed for template: " + templateId);
            return nullptr;
        }

        return templateDef->createWorkflow(params, resourceManager);
    }

    std::shared_ptr<IWorkflowGraph> createGraphFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params
    ) const override {
        auto templateDef = getTemplate(templateId);
        if (!templateDef) {
            LOG_ERROR("[WorkflowTemplateRegistry] Template not found: " + templateId);
            return nullptr;
        }

        // 验证参数
        if (!templateDef->validateParameters(params)) {
            LOG_ERROR("[WorkflowTemplateRegistry] Parameter validation failed for template: " + templateId);
            return nullptr;
        }

        return templateDef->createGraph(params);
    }
};

/**
 * @brief 模板参数构建器实现
 */
class TemplateParameterBuilder : public ITemplateParameterBuilder {
private:
    std::map<std::string, Any> parameters_;

public:
    TemplateParameterBuilder() {
        LOG_INFO("[TemplateParameterBuilder] Created");
    }

    virtual ~TemplateParameterBuilder() = default;

    ITemplateParameterBuilder& addParameter(const TemplateParameter& param) override {
        parameters_[param.name] = param.defaultValue;
        return *this;
    }

    ITemplateParameterBuilder& addParameter(
        const std::string& name,
        const std::string& type,
        Any defaultValue,
        bool required,
        const std::string& description
    ) override {
        parameters_[name] = defaultValue;
        (void)type;  // 未使用但保留参数接口一致性
        (void)required;
        (void)description;
        return *this;
    }

    std::map<std::string, Any> build() const override {
        return parameters_;
    }

    void reset() override {
        parameters_.clear();
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_TEMPLATE_IMPL_H
