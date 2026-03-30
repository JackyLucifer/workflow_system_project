/**
 * @file workflow_template_impl.cpp
 * @brief 业务流程管理系统 - 工作流模板实现
 */

#include "workflow_system/implementations/workflow_template_impl.h"

namespace WorkflowSystem {

// ========== AbstractWorkflowTemplate 实现 ==========

AbstractWorkflowTemplate::AbstractWorkflowTemplate(const std::string& id,
                                                   const std::string& name,
                                                   const std::string& desc,
                                                   const std::string& ver)
    : templateId_(id), templateName_(name),
      description_(desc), version_(ver) {}

AbstractWorkflowTemplate::~AbstractWorkflowTemplate() = default;

std::string AbstractWorkflowTemplate::getTemplateId() const {
    return templateId_;
}

std::string AbstractWorkflowTemplate::getTemplateName() const {
    return templateName_;
}

std::string AbstractWorkflowTemplate::getDescription() const {
    return description_;
}

std::string AbstractWorkflowTemplate::getVersion() const {
    return version_;
}

std::vector<TemplateParameter> AbstractWorkflowTemplate::getParameters() const {
    return parameters_;
}

bool AbstractWorkflowTemplate::validateParameters(const std::map<std::string, Any>& params) const {
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

void AbstractWorkflowTemplate::addParameter(const TemplateParameter& param) {
    parameters_.push_back(param);
}

Any AbstractWorkflowTemplate::getParameter(const std::map<std::string, Any>& params,
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

// ========== WorkflowTemplateRegistry 实现 ==========

WorkflowTemplateRegistry::WorkflowTemplateRegistry() {
    LOG_INFO("[WorkflowTemplateRegistry] Created");
}

WorkflowTemplateRegistry::~WorkflowTemplateRegistry() {
    templates_.clear();
    LOG_INFO("[WorkflowTemplateRegistry] Destroyed");
}

void WorkflowTemplateRegistry::registerTemplate(const std::string& templateId,
                                                std::shared_ptr<IWorkflowTemplate> templateDef) {
    templates_[templateId] = templateDef;
    LOG_INFO("[WorkflowTemplateRegistry] Registered template: " + templateId +
             " (" + templateDef->getTemplateName() + ")");
}

void WorkflowTemplateRegistry::unregisterTemplate(const std::string& templateId) {
    templates_.erase(templateId);
    LOG_INFO("[WorkflowTemplateRegistry] Unregistered template: " + templateId);
}

bool WorkflowTemplateRegistry::hasTemplate(const std::string& templateId) const {
    return templates_.find(templateId) != templates_.end();
}

std::vector<std::string> WorkflowTemplateRegistry::listTemplates() const {
    std::vector<std::string> result;
    for (const auto& pair : templates_) {
        result.push_back(pair.first);
    }
    return result;
}

std::shared_ptr<IWorkflowTemplate> WorkflowTemplateRegistry::getTemplate(
    const std::string& templateId) const {
    auto it = templates_.find(templateId);
    if (it != templates_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<IWorkflow> WorkflowTemplateRegistry::createWorkflowFromTemplate(
    const std::string& templateId,
    const std::map<std::string, Any>& params,
    std::shared_ptr<IResourceManager> resourceManager
) const {
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

std::shared_ptr<IWorkflowGraph> WorkflowTemplateRegistry::createGraphFromTemplate(
    const std::string& templateId,
    const std::map<std::string, Any>& params
) const {
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

// ========== TemplateParameterBuilder 实现 ==========

TemplateParameterBuilder::TemplateParameterBuilder() {
    LOG_INFO("[TemplateParameterBuilder] Created");
}

TemplateParameterBuilder::~TemplateParameterBuilder() = default;

ITemplateParameterBuilder& TemplateParameterBuilder::addParameter(const TemplateParameter& param) {
    parameters_[param.name] = param.defaultValue;
    return *this;
}

ITemplateParameterBuilder& TemplateParameterBuilder::addParameter(
    const std::string& name,
    const std::string& type,
    Any defaultValue,
    bool required,
    const std::string& description) {
    parameters_[name] = defaultValue;
    (void)type;  // 未使用但保留参数接口一致性
    (void)required;
    (void)description;
    return *this;
}

std::map<std::string, Any> TemplateParameterBuilder::build() const {
    return parameters_;
}

void TemplateParameterBuilder::reset() {
    parameters_.clear();
}

} // namespace WorkflowSystem
