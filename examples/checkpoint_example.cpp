/**
 * @file checkpoint_example.cpp
 * @brief 检查点管理器示例程序
 *
 * 功能：
 * - 演示工作流的基本执行
 * - 演示上下文数据的状态保存（模拟检查点概念）
 * - 演示资源管理
 *
 * 注意：完整的检查点功能需要SQLite支持
 * 此示例演示基本的状态保存和恢复概念
 */

#include <iostream>
#include <memory>
#include <workflow_system/core/types.h>
#include <workflow_system/core/logger.h>
#include <workflow_system/core/utils.h>
#include <workflow_system/implementations/system_facade.h>
#include <workflow_system/implementations/workflows/base_workflow.h>
#include <workflow_system/implementations/workflow_context.h>

using namespace WorkflowSystem;

/**
 * @brief 简单工作流示例 - 演示检查点概念
 */
class SimpleWorkflow : public BaseWorkflow {
public:
    explicit SimpleWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("checkpoint_example", resourceManager) {}

protected:
    void onStart() override {
        LOG_INFO("工作流开始执行");
        // 保存初始状态到上下文
        if (context_) {
            context_->set("step1", "completed");
        }
    }

    void onMessage(const IMessage& message) override {
        LOG_INFO("执行工作流，消息: " + message.getTopic());
    }

    void onInterrupt() override {
        LOG_WARNING("工作流被中断");
        // 保存当前状态（模拟检查点）
        if (context_) {
            context_->set("interrupted_at", "step2");
        }
    }

    void onFinalize() override {
        LOG_INFO("工作流执行完成");
        // 保存完成状态
        if (context_) {
            context_->set("all_steps", "completed");
        }
    }

    Any onExecute() override {
        // 模拟工作流步骤
        LOG_INFO("步骤 1: 处理数据");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (context_) {
            context_->set("step1_progress", "100%");
        }

        LOG_INFO("步骤 2: 保存结果");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (context_) {
            context_->set("step2_progress", "100%");
        }

        LOG_INFO("步骤 3: 清理资源");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (context_) {
            context_->set("step3_progress", "100%");
        }

        return Any(std::string("工作流执行成功"));
    }
};

int main() {
    std::cout << "========================================\n";
    std::cout << "  检查点概念示例程序\n";
    std::cout << "========================================\n\n";

    // 初始化系统Facade
    auto& facade = SystemFacade::getInstance();
    facade.initialize("/tmp/workflow_checkpoint_demo");

    // 创建工作流
    auto workflow = std::make_shared<SimpleWorkflow>(facade.getResourceManager());

    std::cout << "========== 测试1: 基本执行 ==========\n";
    auto result = workflow->execute();
    result.wait();
    if (result.valid()) {
        auto value = result.get();
        std::cout << "工作流状态: " << (workflow->getState() == WorkflowState::COMPLETED ? "已完成" : "未完成") << "\n";
    }

    std::cout << "\n========== 测试2: 上下文状态保存 ==========\n";
    // 获取工作流上下文
    auto context = workflow->getContext();
    if (context) {
        auto allData = context->getAllData();
        std::cout << "上下文保存的状态数据:\n";
        for (const auto& pair : allData) {
            std::cout << "  - " << pair.first << " = " << pair.second << "\n";
        }
    }

    std::cout << "\n========== 测试3: 中断和恢复演示 ==========\n";
    auto workflow2 = std::make_shared<SimpleWorkflow>(facade.getResourceManager());
    auto workflowContext2 = std::make_shared<WorkflowContext>(facade.getResourceManager());
    workflow2->setContext(workflowContext2);
    workflow2->start(workflowContext2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    workflow2->interrupt();

    auto context2 = workflow2->getContext();
    if (context2) {
        std::cout << "中断后的状态: " << context2->get("interrupted_at", "未知") << "\n";
    }

    std::cout << "\n========== 说明 ==========\n";
    std::cout << "此示例演示了基本的状态保存和恢复概念。\n";
    std::cout << "完整的检查点功能（持久化到数据库）需要 SQLite 支持。\n";
    std::cout << "实现检查点功能需要：\n";
    std::cout << "  1. ICheckpointManager 接口的完整实现\n";
    std::cout << "  2. SQLite 数据库支持\n";
    std::cout << "  3. CheckpointManagerImpl 实现类\n";

    // 清理资源
    facade.shutdown();

    std::cout << "\n========================================\n";
    std::cout << "  所有测试完成！\n";
    std::cout << "========================================\n";

    return 0;
}
