/**
 * @file workflow_template_example.cpp
 * @brief 业务流程管理系统 - 工作流模板示例
 *
 * 演示功能：
 * - 定义工作流模板
 * - 注册模板到注册表
 * - 从模板创建工作流
 * - 参数化模板实例化
 */

#include "workflow_system/implementations/workflow_template_impl.h"
#include "workflow_system/implementations/system_facade.h"
#include "workflow_system/implementations/workflows/base_workflow.h"
#include "workflow_system/core/logger.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

// ========== 独立工作流类 ==========

/**
 * @brief 数据处理工作流实现
 */
class DataProcessingWorkflow : public BaseWorkflow {
private:
    std::string inputData_;
    std::string operationType_;
    bool enableLogging_;

public:
    DataProcessingWorkflow(const std::string& data,
                      const std::string& operation,
                      bool logging,
                      std::shared_ptr<IResourceManager> rm)
        : BaseWorkflow("DataProcessingWorkflow", rm),
          inputData_(data), operationType_(operation),
          enableLogging_(logging) {}

private:
    void onStart() override {
        if (enableLogging_) {
            std::cout << "  [工作流] 开始执行数据处理模板..." << std::endl;
            std::cout << "  [参数] 输入数据: " << inputData_ << std::endl;
            std::cout << "  [参数] 操作类型: " << operationType_ << std::endl;
        }
    }

    Any onExecute() override {
        if (enableLogging_) {
            std::cout << "  [工作流] 执行操作: " << operationType_ << std::endl;
            std::cout << "  [工作流] 处理数据: " << inputData_ << std::endl;

            // 模拟处理
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        std::string result;
        if (operationType_ == "process") {
            result = "PROCESSED_" + inputData_;
        } else if (operationType_ == "clean") {
            result = "CLEANED_" + inputData_;
        } else if (operationType_ == "validate") {
            result = "VALIDATED_" + inputData_;
        } else {
            result = "DEFAULT_" + inputData_;
        }

        if (enableLogging_) {
            std::cout << "  [工作流] 处理完成: " << result << std::endl;
        }

        getContext()->set("processing_result", result);
        return Any(result);
    }

    void onInterrupt() override {
        std::cout << "  [工作流] 被中断" << std::endl;
    }
};

/**
 * @brief 批处理工作流实现
 */
class BatchProcessingWorkflow : public BaseWorkflow {
private:
    int batchSize_;
    bool parallel_;

public:
    BatchProcessingWorkflow(int batchSize, bool parallel,
                       std::shared_ptr<IResourceManager> rm)
        : BaseWorkflow("BatchProcessingWorkflow", rm),
          batchSize_(batchSize), parallel_(parallel) {}

private:
    void onStart() override {
        std::cout << "  [批处理] 开始批处理，大小: " << batchSize_
                  << ", 并行: " << (parallel_ ? "是" : "否") << std::endl;
    }

    Any onExecute() override {
        std::cout << "  [批处理] 处理批次..." << std::endl;

        // 模拟批处理
        for (int i = 0; i < batchSize_; ++i) {
            std::cout << "    处理项 " << (i + 1) << "/" << batchSize_ << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (parallel_) {
                // 并行处理会更快
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        std::cout << "  [批处理] 批处理完成" << std::endl;

        getContext()->set("batch_count", std::to_string(batchSize_));
        getContext()->set("processed_count", std::to_string(batchSize_));

        return Any(batchSize_);
    }

    void onInterrupt() override {
        std::cout << "  [批处理] 被中断" << std::endl;
    }
};

// ========== 简单工作流模板 ==========

/**
 * @brief 数据处理模板
 *
 * 处理数据的简单工作流模板
 */
class SimpleDataProcessingTemplate : public IWorkflowTemplate {
private:
    std::vector<TemplateParameter> parameters_;

public:
    SimpleDataProcessingTemplate() {
        // 添加参数
        TemplateParameter param1;
        param1.name = "input_data";
        param1.type = "string";
        param1.defaultValue = Any(std::string("default_data"));
        param1.required = true;
        param1.description = "要处理的数据";
        parameters_.push_back(param1);

        TemplateParameter param2;
        param2.name = "operation_type";
        param2.type = "string";
        param2.defaultValue = Any(std::string("process"));
        param2.required = false;
        param2.description = "操作类型：process, clean, validate";
        parameters_.push_back(param2);

        TemplateParameter param3;
        param3.name = "enable_logging";
        param3.type = "bool";
        param3.defaultValue = Any(true);
        param3.required = false;
        param3.description = "是否启用日志";
        parameters_.push_back(param3);
    }

    virtual ~SimpleDataProcessingTemplate() = default;

    // ========== 模板元数据 ==========

    std::string getTemplateId() const override {
        return "data_processing";
    }

    std::string getTemplateName() const override {
        return "数据处理模板";
    }

    std::string getDescription() const override {
        return "用于数据清洗和处理的模板";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    // ========== 参数管理 ==========

    std::vector<TemplateParameter> getParameters() const override {
        return parameters_;
    }

    bool validateParameters(const std::map<std::string, Any>& params) const override {
        // 检查必填参数
        for (const auto& param : parameters_) {
            if (param.required && params.find(param.name) == params.end()) {
                LOG_WARNING("[SimpleDataProcessingTemplate] Required parameter '" +
                         param.name + "' is missing");
                return false;
            }
        }

        return true;
    }

    // ========== 工作流创建 ==========

    std::shared_ptr<IWorkflow> createWorkflow(
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager
    ) const override {

        // 获取参数
        std::string inputData = params.find("input_data") != params.end() ?
            params.at("input_data").cast<std::string>() : "default_data";
        std::string operationType = params.find("operation_type") != params.end() ?
            params.at("operation_type").cast<std::string>() : "process";
        bool enableLogging = params.find("enable_logging") != params.end() ?
            params.at("enable_logging").cast<bool>() : true;

        // 创建具体工作流
        auto workflow = std::make_shared<DataProcessingWorkflow>(
            inputData, operationType, enableLogging, resourceManager);

        return std::static_pointer_cast<IWorkflow>(workflow);
    }

    std::shared_ptr<IWorkflowGraph> createGraph(
        const std::map<std::string, Any>&
    ) const override {
        // 创建简单的工作流图
        auto graph = std::make_shared<WorkflowGraph>();

        auto startNode = std::make_shared<WorkflowNode>("start", "Start Node", NodeType::START);
        auto taskNode = std::make_shared<WorkflowNode>("process_task", "Process Task", NodeType::TASK);
        auto endNode = std::make_shared<WorkflowNode>("end", "End Node", NodeType::END);

        graph->addNode(startNode);
        graph->addNode(taskNode);
        graph->addNode(endNode);

        graph->addEdge(startNode->getId(), taskNode->getId(), "start_to_process");
        graph->addEdge(taskNode->getId(), endNode->getId(), "process_to_end");

        return graph;
    }
};

/**
 * @brief 批处理模板
 *
 * 批量处理多个数据项的工作流模板
 */
class SimpleBatchProcessingTemplate : public IWorkflowTemplate {
private:
    std::vector<TemplateParameter> parameters_;

public:
    SimpleBatchProcessingTemplate() {
        TemplateParameter param1;
        param1.name = "batch_size";
        param1.type = "int";
        param1.defaultValue = Any(10);
        param1.required = false;
        param1.description = "批处理大小";
        parameters_.push_back(param1);

        TemplateParameter param2;
        param2.name = "parallel";
        param2.type = "bool";
        param2.defaultValue = Any(false);
        param2.required = false;
        param2.description = "是否并行处理";
        parameters_.push_back(param2);
    }

    virtual ~SimpleBatchProcessingTemplate() = default;

    // ========== 模板元数据 ==========

    std::string getTemplateId() const override {
        return "batch_processing";
    }

    std::string getTemplateName() const override {
        return "批处理模板";
    }

    std::string getDescription() const override {
        return "用于批量处理数据的模板";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    // ========== 参数管理 ==========

    std::vector<TemplateParameter> getParameters() const override {
        return parameters_;
    }

    bool validateParameters(const std::map<std::string, Any>& params) const override {
        // 检查必填参数
        for (const auto& param : parameters_) {
            if (param.required && params.find(param.name) == params.end()) {
                LOG_WARNING("[SimpleBatchProcessingTemplate] Required parameter '" +
                         param.name + "' is missing");
                return false;
            }
        }

        return true;
    }

    // ========== 工作流创建 ==========

    std::shared_ptr<IWorkflow> createWorkflow(
        const std::map<std::string, Any>& params,
        std::shared_ptr<IResourceManager> resourceManager
    ) const override {

        int batchSize = params.find("batch_size") != params.end() ?
            params.at("batch_size").cast<int>() : 10;
        bool parallel = params.find("parallel") != params.end() ?
            params.at("parallel").cast<bool>() : false;

        // 创建 BatchProcessingWorkflow 的实例
        auto workflow = std::make_shared<BatchProcessingWorkflow>(
            batchSize, parallel, resourceManager);

        return workflow;
    }

    std::shared_ptr<IWorkflowGraph> createGraph(
        const std::map<std::string, Any>&
    ) const override {
        auto graph = std::make_shared<WorkflowGraph>();

        auto startNode = std::make_shared<WorkflowNode>("start", "Start Node", NodeType::START);
        auto batchNode = std::make_shared<WorkflowNode>("batch_task", "Batch Task", NodeType::TASK);
        auto endNode = std::make_shared<WorkflowNode>("end", "End Node", NodeType::END);

        graph->addNode(startNode);
        graph->addNode(batchNode);
        graph->addNode(endNode);

        graph->addEdge(startNode->getId(), batchNode->getId(), "start_to_batch");
        graph->addEdge(batchNode->getId(), endNode->getId(), "batch_to_end");

        return graph;
    }
};

// ========== 辅助函数 ==========

void printSeparator() {
    std::cout << "\n========================================\n" << std::endl;
}

void printTemplateInfo(std::shared_ptr<IWorkflowTemplate> templateDef) {
    std::cout << "  - ID: " << templateDef->getTemplateId() << std::endl;
    std::cout << "    名称: " << templateDef->getTemplateName() << std::endl;
    std::cout << "    版本: " << templateDef->getVersion() << std::endl;
    std::cout << "    描述: " << templateDef->getDescription() << std::endl;

    auto params = templateDef->getParameters();
    if (!params.empty()) {
        std::cout << "    参数:" << std::endl;
        for (const auto& param : params) {
            std::cout << "      - " << param.name << " (" << param.type << ")";
            if (param.required) {
                std::cout << " [必填]";
            }
            std::cout << std::endl;
            std::cout << "        描述: " << param.description << std::endl;
        }
    }
}

// ========== 主函数 ==========

int main() {
    std::cout << "\n========== 工作流模板示例 ==========\n" << std::endl;

    // 创建模板注册表
    WorkflowTemplateRegistry registry;

    // ========== 注册模板 ==========

    std::cout << "\n[1] 注册工作流模板..." << std::endl;

    auto dataProcessingTemplate = std::make_shared<SimpleDataProcessingTemplate>();
    auto batchProcessingTemplate = std::make_shared<SimpleBatchProcessingTemplate>();

    registry.registerTemplate("data_processing", dataProcessingTemplate);
    registry.registerTemplate("batch_processing", batchProcessingTemplate);

    std::cout << "已注册的模板：" << std::endl;
    auto templateIds = registry.listTemplates();
    for (const auto& id : templateIds) {
        auto templateDef = registry.getTemplate(id);
        if (templateDef) {
            printTemplateInfo(templateDef);
        }
    }

    // ========== 示例1：使用数据处理模板 ==========

    std::cout << "\n[示例1] 使用数据处理模板..." << std::endl;

    auto& facade = SystemFacade::getInstance();
    auto context1 = std::make_shared<WorkflowContext>(facade.getResourceManager());

    // 使用默认参数
    auto workflow1 = registry.createWorkflowFromTemplate(
        "data_processing",
        {},
        facade.getResourceManager()
    );

    if (workflow1) {
        workflow1->setContext(context1);
        workflow1->start(context1);

        auto future1 = workflow1->execute();
        future1.wait();

        std::cout << "结果: " << context1->get("processing_result", "N/A") << std::endl;
    }

    // ========== 示例2：使用自定义参数 ==========

    std::cout << "\n[示例2] 使用自定义参数..." << std::endl;

    auto context2 = std::make_shared<WorkflowContext>(facade.getResourceManager());

    // 使用自定义参数
    std::map<std::string, Any> params2;
    params2["input_data"] = std::string("custom_data_456");
    params2["operation_type"] = std::string("clean");
    params2["enable_logging"] = true;

    auto workflow2 = registry.createWorkflowFromTemplate(
        "data_processing",
        params2,
        facade.getResourceManager()
    );

    if (workflow2) {
        workflow2->setContext(context2);
        workflow2->start(context2);

        auto future2 = workflow2->execute();
        future2.wait();

        std::cout << "结果: " << context2->get("processing_result", "N/A") << std::endl;
    }

    // ========== 示例3：使用批处理模板 ==========

    std::cout << "\n[示例3] 使用批处理模板..." << std::endl;

    auto context3 = std::make_shared<WorkflowContext>(facade.getResourceManager());

    std::map<std::string, Any> params3;
    params3["batch_size"] = 5;
    params3["parallel"] = false;

    auto workflow3 = registry.createWorkflowFromTemplate(
        "batch_processing",
        params3,
        facade.getResourceManager()
    );

    if (workflow3) {
        workflow3->setContext(context3);
        workflow3->start(context3);

        auto future3 = workflow3->execute();
        future3.wait();

        std::cout << "处理项数: " << context3->get("batch_count", "0") << std::endl;
    }

    // ========== 示例4：参数验证 ==========

    std::cout << "\n[示例4] 参数验证..." << std::endl;

    auto context4 = std::make_shared<WorkflowContext>(facade.getResourceManager());

    // 缺少必填参数
    std::map<std::string, Any> params4;
    // params4["input_data"] = std::string("test_data");  // 注释掉以测试验证

    auto workflow4 = registry.createWorkflowFromTemplate(
        "data_processing",
        params4,
        facade.getResourceManager()
    );

    if (!workflow4) {
        std::cout << "正确：参数验证失败，工作流未创建" << std::endl;
    } else {
        workflow4->setContext(context4);
        workflow4->start(context4);
        auto future4 = workflow4->execute();
        future4.wait();
    }

    // ========== 示例5：使用模板参数构建器 ==========

    std::cout << "\n[示例5] 使用参数构建器..." << std::endl;

    TemplateParameterBuilder builder;
    builder.addParameter("input_data", "string", Any(std::string("builder_data")), true, "输入数据");
    builder.addParameter("operation_type", "string", Any(std::string("validate")), false, "操作类型");

    auto params5 = builder.build();

    std::cout << "构建的参数：" << std::endl;
    for (const auto& pair : params5) {
        std::cout << "  " << pair.first << ": " << pair.second.cast<std::string>() << std::endl;
    }

    auto context5 = std::make_shared<WorkflowContext>(facade.getResourceManager());

    auto workflow5 = registry.createWorkflowFromTemplate(
        "data_processing",
        params5,
        facade.getResourceManager()
    );

    if (workflow5) {
        workflow5->setContext(context5);
        workflow5->start(context5);

        auto future5 = workflow5->execute();
        future5.wait();

        std::cout << "结果: " << context5->get("processing_result", "N/A") << std::endl;
    }

    // ========== 总结 ==========

    printSeparator();
    std::cout << "工作流模板演示完成！" << std::endl;
    std::cout << "\n功能特性：" << std::endl;
    std::cout << "  1. 定义可重用的工作流模板" << std::endl;
    std::cout << "  2. 参数化模板实例化" << std::endl;
    std::cout << "  3. 模板注册表管理" << std::endl;
    std::cout << "  4. 参数验证" << std::endl;
    std::cout << "  5. 从模板创建工作流和图" << std::endl;
    std::cout << "  6. 模板参数构建器" << std::endl;
    printSeparator();

    return 0;
}
