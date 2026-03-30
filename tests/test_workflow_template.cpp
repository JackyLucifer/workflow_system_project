/**
 * @file test_workflow_template.cpp
 * @brief 工作流模板测试
 */

#include "workflow_system/implementations/workflow_template_impl.h"
#include "workflow_system/implementations/workflow_graph.h"
#include "workflow_system/implementations/filesystem_resource_manager.h"
#include "test_framework.h"

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助类 ==========

class TestWorkflowTemplate : public IWorkflowTemplate {
private:
    std::string templateId_;
    std::string templateName_;
    std::string description_;
    std::string version_;
    std::vector<TemplateParameter> parameters_;

public:
    TestWorkflowTemplate(const std::string& id, const std::string& name) {
        templateId_ = id;
        templateName_ = name;
        description_ = "Test workflow template";
        version_ = "1.0";

        // 添加默认参数
        TemplateParameter param1;
        param1.name = "param1";
        param1.type = "string";
        param1.required = true;
        param1.description = "First parameter";
        parameters_.push_back(param1);

        TemplateParameter param2;
        param2.name = "param2";
        param2.type = "int";
        param2.required = false;
        param2.description = "Second parameter";
        parameters_.push_back(param2);
    }

    std::string getTemplateId() const override { return templateId_; }
    std::string getTemplateName() const override { return templateName_; }
    std::string getDescription() const override { return description_; }
    std::string getVersion() const override { return version_; }

    std::vector<TemplateParameter> getParameters() const override { return parameters_; }

    bool validateParameters(const std::map<std::string, Any>& params) const override {
        for (const auto& param : parameters_) {
            if (param.required) {
                if (params.find(param.name) == params.end()) {
                    return false;
                }
            }
        }
        return true;
    }

    std::shared_ptr<IWorkflow> createWorkflow(
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager) const override {
        // 简单返回 nullptr，表示创建了工作流
        return nullptr;
    }

    std::shared_ptr<IWorkflowGraph> createGraph(
        const std::map<std::string, Any>& params) const override {
        auto graph = std::make_shared<WorkflowGraph>();
        graph->setName(templateName_);

        auto node1 = std::make_shared<WorkflowNode>("node1", "Node1");
        auto node2 = std::make_shared<WorkflowNode>("node2", "Node2");

        graph->addNode(node1);
        graph->addNode(node2);
        graph->addEdge("node1", "node2", "");

        return graph;
    }
};

class TestTemplateRegistry : public IWorkflowTemplateRegistry {
private:
    std::map<std::string, std::shared_ptr<IWorkflowTemplate>> templates_;

public:
    void registerTemplate(const std::string& templateId,
                         std::shared_ptr<IWorkflowTemplate> templateDef) override {
        templates_[templateId] = templateDef;
    }

    void unregisterTemplate(const std::string& templateId) override {
        templates_.erase(templateId);
    }

    bool hasTemplate(const std::string& templateId) const override {
        return templates_.find(templateId) != templates_.end();
    }

    std::vector<std::string> listTemplates() const override {
        std::vector<std::string> names;
        for (const auto& pair : templates_) {
            names.push_back(pair.first);
        }
        return names;
    }

    std::shared_ptr<IWorkflowTemplate> getTemplate(const std::string& templateId) const override {
        auto it = templates_.find(templateId);
        if (it != templates_.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<IWorkflow> createWorkflowFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager) const override {
        auto tmpl = getTemplate(templateId);
        if (tmpl) {
            return tmpl->createWorkflow(params, resourceManager);
        }
        return nullptr;
    }

    std::shared_ptr<IWorkflowGraph> createGraphFromTemplate(
        const std::string& templateId,
        const std::map<std::string, Any>& params) const override {
        auto tmpl = getTemplate(templateId);
        if (tmpl) {
            return tmpl->createGraph(params);
        }
        return nullptr;
    }
};

// ========== 测试用例 ==========

void testTemplate_BasicProperties() {
    TestWorkflowTemplate tmpl("tpl_001", "TestTemplate");

    TEST_ASSERT_STRING_EQUAL("tpl_001", tmpl.getTemplateId().c_str());
    TEST_ASSERT_STRING_EQUAL("TestTemplate", tmpl.getTemplateName().c_str());
    TEST_ASSERT_FALSE(tmpl.getDescription().empty());
    TEST_ASSERT_STRING_EQUAL("1.0", tmpl.getVersion().c_str());
}

void testTemplate_GetParameters() {
    TestWorkflowTemplate tmpl("tpl_001", "TestTemplate");

    auto params = tmpl.getParameters();

    TEST_ASSERT_EQUAL(2, static_cast<int>(params.size()));
    TEST_ASSERT_STRING_EQUAL("param1", params[0].name.c_str());
    TEST_ASSERT_STRING_EQUAL("param2", params[1].name.c_str());
}

void testTemplate_ValidateParameters_Success() {
    TestWorkflowTemplate tmpl("tpl_001", "TestTemplate");

    std::map<std::string, Any> params;
    params["param1"] = Any(std::string("value1"));
    params["param2"] = Any(42);

    bool valid = tmpl.validateParameters(params);

    TEST_ASSERT_TRUE(valid);
}

void testTemplate_ValidateParameters_MissingRequired() {
    TestWorkflowTemplate tmpl("tpl_001", "TestTemplate");

    std::map<std::string, Any> params;
    // param1 是必填的，但没有提供
    params["param2"] = Any(42);

    bool valid = tmpl.validateParameters(params);

    TEST_ASSERT_FALSE(valid);
}

void testTemplate_CreateGraph() {
    TestWorkflowTemplate tmpl("tpl_001", "TestTemplate");

    std::map<std::string, Any> params;
    auto graph = tmpl.createGraph(params);

    TEST_ASSERT_TRUE(graph != nullptr);
    TEST_ASSERT_EQUAL(2, graph->getNodeCount());
    TEST_ASSERT_EQUAL(1, graph->getEdgeCount());
}

void testRegistry_RegisterTemplate() {
    TestTemplateRegistry registry;
    auto tmpl = std::make_shared<TestWorkflowTemplate>("tpl_001", "Template1");

    registry.registerTemplate("tpl_001", tmpl);

    TEST_ASSERT_TRUE(registry.hasTemplate("tpl_001"));
}

void testRegistry_UnregisterTemplate() {
    TestTemplateRegistry registry;
    auto tmpl = std::make_shared<TestWorkflowTemplate>("tpl_001", "Template1");

    registry.registerTemplate("tpl_001", tmpl);
    TEST_ASSERT_TRUE(registry.hasTemplate("tpl_001"));

    registry.unregisterTemplate("tpl_001");
    TEST_ASSERT_FALSE(registry.hasTemplate("tpl_001"));
}

void testRegistry_ListTemplates() {
    TestTemplateRegistry registry;

    registry.registerTemplate("tpl_001", std::make_shared<TestWorkflowTemplate>("tpl_001", "Template1"));
    registry.registerTemplate("tpl_002", std::make_shared<TestWorkflowTemplate>("tpl_002", "Template2"));
    registry.registerTemplate("tpl_003", std::make_shared<TestWorkflowTemplate>("tpl_003", "Template3"));

    auto templates = registry.listTemplates();

    TEST_ASSERT_EQUAL(3, static_cast<int>(templates.size()));
}

void testRegistry_GetTemplate() {
    TestTemplateRegistry registry;
    auto tmpl = std::make_shared<TestWorkflowTemplate>("tpl_001", "Template1");

    registry.registerTemplate("tpl_001", tmpl);

    auto retrieved = registry.getTemplate("tpl_001");

    TEST_ASSERT_TRUE(retrieved != nullptr);
    TEST_ASSERT_STRING_EQUAL("tpl_001", retrieved->getTemplateId().c_str());
}

void testRegistry_CreateGraphFromTemplate() {
    TestTemplateRegistry registry;
    auto tmpl = std::make_shared<TestWorkflowTemplate>("tpl_001", "Template1");

    registry.registerTemplate("tpl_001", tmpl);

    std::map<std::string, Any> params;
    auto graph = registry.createGraphFromTemplate("tpl_001", params);

    TEST_ASSERT_TRUE(graph != nullptr);
    TEST_ASSERT_EQUAL(2, graph->getNodeCount());
}

// ========== 测试套件创建 ==========

TestSuite createWorkflowTemplateTestSuite() {
    TestSuite suite("工作流模板测试");

    suite.addTest("Template_BasicProperties", testTemplate_BasicProperties);
    suite.addTest("Template_GetParameters", testTemplate_GetParameters);
    suite.addTest("Template_ValidateParameters_Success", testTemplate_ValidateParameters_Success);
    suite.addTest("Template_ValidateParameters_MissingRequired", testTemplate_ValidateParameters_MissingRequired);
    suite.addTest("Template_CreateGraph", testTemplate_CreateGraph);
    suite.addTest("Registry_RegisterTemplate", testRegistry_RegisterTemplate);
    suite.addTest("Registry_UnregisterTemplate", testRegistry_UnregisterTemplate);
    suite.addTest("Registry_ListTemplates", testRegistry_ListTemplates);
    suite.addTest("Registry_GetTemplate", testRegistry_GetTemplate);
    suite.addTest("Registry_CreateGraphFromTemplate", testRegistry_CreateGraphFromTemplate);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowTemplateTestSuite());

    return TestRunner::runAllSuites(suites);
}
