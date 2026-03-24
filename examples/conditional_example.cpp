/**
 * @file conditional_example.cpp
 * @brief 业务流程管理系统 - 条件分支工作流示例
 *
 * 演示功能：
 * - 创建条件分支工作流
 * - 定义多个分支和条件
 * - 根据运行时条件选择执行路径
 * - 设置默认分支
 */

#include "implementations/conditional_workflow_impl.h"
#include "implementations/workflows/base_workflow.h"
#include "implementations/system_facade.h"
#include "core/logger.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

// ========== 简单任务工作流 ==========

/**
 * @brief 数据验证工作流
 */
class DataValidationWorkflow : public BaseWorkflow {
public:
    DataValidationWorkflow(std::shared_ptr<IResourceManager> rm)
        : BaseWorkflow("DataValidation", rm) {}

private:
    void onStart() override {
        std::cout << "  [数据验证] 开始验证数据..." << std::endl;
    }

    Any onExecute() override {
        // 从上下文获取数据
        std::string data = getContext()->get("data", "");
        bool isValid = !data.empty() && data.size() > 0;

        std::cout << "  [数据验证] 数据: " << data;
        std::cout << ", 结果: " << (isValid ? "有效" : "无效") << std::endl;

        // 将验证结果写入上下文
        getContext()->set("data_valid", isValid ? "true" : "false");

        return Any(isValid);
    }

    void onInterrupt() override {
        std::cout << "  [数据验证] 被中断" << std::endl;
    }
};

/**
 * @brief 数据处理工作流（有效数据）
 */
class DataProcessingWorkflow : public BaseWorkflow {
public:
    DataProcessingWorkflow(std::shared_ptr<IResourceManager> rm)
        : BaseWorkflow("DataProcessing", rm) {}

private:
    void onStart() override {
        std::cout << "  [数据处理] 开始处理有效数据..." << std::endl;
    }

    Any onExecute() override {
        std::string data = getContext()->get("data", "");
        std::cout << "  [数据处理] 处理: " << data << std::endl;

        // 模拟处理
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::string result = "PROCESSED_" + data;
        getContext()->set("processing_result", result);

        std::cout << "  [数据处理] 处理完成: " << result << std::endl;
        return Any(result);
    }

    void onInterrupt() override {
        std::cout << "  [数据处理] 被中断" << std::endl;
    }
};

/**
 * @brief 数据清洗工作流（无效数据）
 */
class DataCleaningWorkflow : public BaseWorkflow {
public:
    DataCleaningWorkflow(std::shared_ptr<IResourceManager> rm)
        : BaseWorkflow("DataCleaning", rm) {}

private:
    void onStart() override {
        std::cout << "  [数据清洗] 开始清洗无效数据..." << std::endl;
    }

    Any onExecute() override {
        std::string data = getContext()->get("data", "");
        std::cout << "  [数据清洗] 清洗: " << data << std::endl;

        // 模拟清洗
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        std::string result = "CLEANED_" + data;
        getContext()->set("cleaning_result", result);

        std::cout << "  [数据清洗] 清洗完成: " << result << std::endl;
        return Any(result);
    }

    void onInterrupt() override {
        std::cout << "  [数据清洗] 被中断" << std::endl;
    }
};

/**
 * @brief 数据归档工作流（默认分支）
 */
class DataArchivingWorkflow : public BaseWorkflow {
public:
    DataArchivingWorkflow(std::shared_ptr<IResourceManager> rm)
        : BaseWorkflow("DataArchiving", rm) {}

private:
    void onStart() override {
        std::cout << "  [数据归档] 开始归档数据..." << std::endl;
    }

    Any onExecute() override {
        std::string data = getContext()->get("data", "");
        std::cout << "  [数据归档] 归档: " << data << std::endl;

        // 模拟归档
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::string result = "ARCHIVED_" + data;
        getContext()->set("archive_result", result);

        std::cout << "  [数据归档] 归档完成: " << result << std::endl;
        return Any(result);
    }

    void onInterrupt() override {
        std::cout << "  [数据归档] 被中断" << std::endl;
    }
};

// ========== 辅助函数 ==========

void printSeparator() {
    std::cout << "\n========================================\n" << std::endl;
}

void printBranchInfo(const Branch& branch) {
    std::cout << "  - 名称: " << branch.name << std::endl;
    std::cout << "    条件: " << branch.conditionName << std::endl;
    std::cout << "    优先级: " << branch.priority << std::endl;
    std::cout << "    描述: " << branch.description << std::endl;
}

// ========== 主函数 ==========

int main() {
    std::cout << "\n========== 条件分支工作流示例 ==========\n" << std::endl;

    // 创建系统门面
    auto& facade = SystemFacade::getInstance();

    // ========== 创建条件分支工作流 ==========
    std::cout << "\n[1] 创建条件分支工作流..." << std::endl;

    // ========== 测试场景1：有效数据 ==========
    std::cout << "\n[场景1] 处理有效数据..." << std::endl;

    // 为每个场景创建独立的上下文
    auto context1 = std::make_shared<WorkflowContext>(facade.getResourceManager());
    context1->set("data", "valid_data_123");
    context1->set("data_valid", "true");  // 直接设置验证结果

    // 创建场景1的工作流实例
    auto conditional1 = std::make_shared<ConditionalWorkflow>(facade.getResourceManager());
    auto processing1 = std::make_shared<DataProcessingWorkflow>(facade.getResourceManager());
    auto archiving1 = std::make_shared<DataArchivingWorkflow>(facade.getResourceManager());

    // 清空并添加分支
    Branch validDataBranch;
    validDataBranch.name = "ValidDataPath";
    validDataBranch.conditionName = "is_data_valid";
    validDataBranch.condition = [context1]() -> bool {
        std::string valid = context1->get("data_valid", "false");
        return valid == "true";
    };
    validDataBranch.workflow = processing1;
    validDataBranch.priority = 10;
    validDataBranch.description = "数据有效时的处理路径";
    conditional1->addBranch(validDataBranch);

    // 设置默认分支
    Branch defaultBranch;
    defaultBranch.name = "DefaultPath";
    defaultBranch.workflow = archiving1;
    defaultBranch.priority = -1;
    defaultBranch.description = "默认的归档路径";
    conditional1->setDefaultBranch(defaultBranch);

    std::cout << "\n已定义的分支：" << std::endl;
    std::vector<Branch> branches = conditional1->getAllBranches();
    for (size_t i = 0; i < branches.size(); ++i) {
        printBranchInfo(branches[i]);
    }
    printBranchInfo(*conditional1->getDefaultBranch());

    // 执行条件工作流
    conditional1->setContext(context1);
    conditional1->start(context1);
    conditional1->evaluateAndExecute();

    // 启动执行并等待完成
    auto future1 = conditional1->execute();
    future1.wait();

    std::cout << "执行完成，结果: " << context1->get("processing_result", "N/A") << std::endl;

    // ========== 测试场景2：无效数据 ==========
    std::cout << "\n[场景2] 处理无效数据..." << std::endl;

    // 为每个场景创建独立的上下文
    auto context2 = std::make_shared<WorkflowContext>(facade.getResourceManager());
    context2->set("data", "");
    context2->set("data_valid", "false");  // 直接设置验证结果

    // 创建场景2的工作流实例
    auto conditional2 = std::make_shared<ConditionalWorkflow>(facade.getResourceManager());
    auto cleaning2 = std::make_shared<DataCleaningWorkflow>(facade.getResourceManager());
    auto processing2 = std::make_shared<DataProcessingWorkflow>(facade.getResourceManager());
    auto archiving2 = std::make_shared<DataArchivingWorkflow>(facade.getResourceManager());

    Branch invalidDataBranch;
    invalidDataBranch.name = "InvalidDataPath";
    invalidDataBranch.conditionName = "is_data_valid";
    invalidDataBranch.condition = [context2]() -> bool {
        std::string valid = context2->get("data_valid", "false");
        return valid == "false" || valid == "";
    };
    invalidDataBranch.workflow = cleaning2;
    invalidDataBranch.priority = 10;
    invalidDataBranch.description = "数据无效时的清洗路径";
    conditional2->addBranch(invalidDataBranch);

    Branch validDataBranch2;
    validDataBranch2.name = "ValidDataPath";
    validDataBranch2.conditionName = "is_data_valid";
    validDataBranch2.condition = [context2]() -> bool {
        std::string valid = context2->get("data_valid", "false");
        return valid == "true";
    };
    validDataBranch2.workflow = processing2;
    validDataBranch2.priority = 10;
    validDataBranch2.description = "数据有效时的处理路径";
    conditional2->addBranch(validDataBranch2);

    Branch defaultBranch2;
    defaultBranch2.name = "DefaultPath";
    defaultBranch2.workflow = archiving2;
    defaultBranch2.priority = -1;
    defaultBranch2.description = "默认的归档路径";
    conditional2->setDefaultBranch(defaultBranch2);

    std::cout << "\n已定义的分支：" << std::endl;
    branches = conditional2->getAllBranches();
    for (size_t i = 0; i < branches.size(); ++i) {
        printBranchInfo(branches[i]);
    }

    conditional2->setContext(context2);
    conditional2->start(context2);
    conditional2->evaluateAndExecute();

    // 启动执行并等待完成
    auto future2 = conditional2->execute();
    future2.wait();

    std::cout << "执行完成，结果: " << context2->get("cleaning_result", "N/A") << std::endl;

    // ========== 测试场景3：多优先级条件 ==========
    std::cout << "\n[场景3] 多优先级条件..." << std::endl;

    // 为每个场景创建独立的上下文
    auto context3 = std::make_shared<WorkflowContext>(facade.getResourceManager());
    context3->set("data", "urgent_data");
    context3->set("data_size", "large");
    context3->set("user_role", "admin");

    // 创建场景3的工作流实例
    auto conditional3 = std::make_shared<ConditionalWorkflow>(facade.getResourceManager());
    auto processing3a = std::make_shared<DataProcessingWorkflow>(facade.getResourceManager());
    auto processing3b = std::make_shared<DataProcessingWorkflow>(facade.getResourceManager());
    auto validation3 = std::make_shared<DataValidationWorkflow>(facade.getResourceManager());
    auto archiving3 = std::make_shared<DataArchivingWorkflow>(facade.getResourceManager());

    // 高优先级分支
    Branch urgentAdminBranch;
    urgentAdminBranch.name = "UrgentAdminPath";
    urgentAdminBranch.conditionName = "is_urgent_admin";
    urgentAdminBranch.condition = [context3]() -> bool {
        std::string data = context3->get("data", "");
        std::string size = context3->get("data_size", "");
        std::string role = context3->get("user_role", "");
        size_t found = data.find("urgent");
        return (found != std::string::npos) && (size == "large") && (role == "admin");
    };
    urgentAdminBranch.workflow = processing3a;
    urgentAdminBranch.priority = 100;
    urgentAdminBranch.description = "紧急数据+管理员的高优先级路径";
    conditional3->addBranch(urgentAdminBranch);

    // 中优先级分支
    Branch normalAdminBranch;
    normalAdminBranch.name = "NormalAdminPath";
    normalAdminBranch.conditionName = "is_normal_admin";
    normalAdminBranch.condition = [context3]() -> bool {
        std::string role = context3->get("user_role", "");
        return role == "admin";
    };
    normalAdminBranch.workflow = processing3b;
    normalAdminBranch.priority = 50;
    normalAdminBranch.description = "管理员的中优先级路径";
    conditional3->addBranch(normalAdminBranch);

    // 低优先级分支
    Branch userBranch;
    userBranch.name = "UserPath";
    userBranch.conditionName = "is_user";
    userBranch.condition = [context3]() -> bool {
        std::string role = context3->get("user_role", "");
        return role == "user" || role == "";
    };
    userBranch.workflow = validation3;
    userBranch.priority = 10;
    userBranch.description = "普通用户的低优先级路径";
    conditional3->addBranch(userBranch);

    Branch defaultBranch3;
    defaultBranch3.name = "DefaultPath";
    defaultBranch3.workflow = archiving3;
    defaultBranch3.priority = -1;
    defaultBranch3.description = "默认的归档路径";
    conditional3->setDefaultBranch(defaultBranch3);

    std::cout << "\n已定义的分支：" << std::endl;
    branches = conditional3->getAllBranches();
    for (size_t i = 0; i < branches.size(); ++i) {
        printBranchInfo(branches[i]);
    }

    conditional3->setContext(context3);
    conditional3->start(context3);
    conditional3->evaluateAndExecute();

    // 启动执行并等待完成
    auto future3 = conditional3->execute();
    future3.wait();

    std::cout << "执行完成" << std::endl;

    // ========== 总结 ==========
    printSeparator();
    std::cout << "条件分支工作流演示完成！" << std::endl;
    std::cout << "\n功能特性：" << std::endl;
    std::cout << "  1. 根据条件动态选择执行分支" << std::endl;
    std::cout << "  2. 支持多优先级分支" << std::endl;
    std::cout << "  3. 支持默认分支" << std::endl;
    std::cout << "  4. 上下文数据在工作流间传递" << std::endl;
    printSeparator();

    // 智能指针自动清理，无需手动 delete

    return 0;
}
