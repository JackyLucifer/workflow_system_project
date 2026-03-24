/**
 * @file checkpoint_example.cpp
 * @brief 检查点管理器示例程序
 *
 * 功能：
 * - 演示如何使用检查点保存工作流状态
 * - 演示如何从检查点恢复工作流
 * - 演示检查点的清理和管理
 */

#include <iostream>
#include <memory>
#include <workflow_system/interfaces/workflow.h>
#include <workflow_system/interfaces/checkpoint_manager.h>
#include <workflow_system/core/logger.h>

using namespace WorkflowSystem;

/**
 * @brief 简单工作流示例
 */
class SimpleWorkflow : public BaseWorkflow {
public:
    SimpleWorkflow() : BaseWorkflow("checkpoint_example") {}

    void execute(const IMessage& message) override {
        LOG_INFO("执行工作流，消息: " + message.getContent());

        // 模拟工作流步骤
        LOG_INFO("步骤 1: 处理数据");
        LOG_INFO("步骤 2: 保存结果");
        LOG_INFO("步骤 3: 清理资源");

        markAsCompleted();
    }
};

int main() {
    std::cout << "========================================\n";
    std::cout << "  检查点管理示例程序\n";
    std::cout << "========================================\n\n";

    // 创建检查点管理器
    auto checkpointMgr = createCheckpointManager();

    // 创建工作流
    auto workflow = std::make_shared<SimpleWorkflow>();

    std::cout << "========== 测试1: 基本执行 ==========\n";
    Message msg("test_message");
    workflow->execute(msg);
    std::cout << "工作流状态: " << (workflow->isCompleted() ? "已完成" : "未完成") << "\n";

    std::cout << "\n========== 测试2: 保存检查点 ==========\n";
    if (checkpointMgr) {
        bool success = checkpointMgr->saveCheckpoint(workflow, "checkpoint_1");
        std::cout << "检查点保存" << (success ? "成功" : "失败") << "\n";
    } else {
        std::cout << "检查点管理器未初始化（可能需要 SQLite）\n";
    }

    std::cout << "\n========== 测试3: 列出检查点 ==========\n";
    if (checkpointMgr) {
        auto checkpoints = checkpointMgr->listCheckpoints();
        std::cout << "找到 " << checkpoints.size() << " 个检查点:\n";
        for (const auto& cp : checkpoints) {
            std::cout << "  - " << cp << "\n";
        }
    }

    std::cout << "\n========== 测试4: 恢复检查点 ==========\n";
    if (checkpointMgr) {
        auto restored = checkpointMgr->restoreCheckpoint("checkpoint_1", workflow);
        if (restored) {
            std::cout << "工作流已从检查点恢复\n";
            std::cout << "恢复后状态: " << (workflow->isCompleted() ? "已完成" : "未完成") << "\n";
        } else {
            std::cout << "检查点恢复失败（可能需要 SQLite）\n";
        }
    }

    std::cout << "\n========== 测试5: 清理检查点 ==========\n";
    if (checkpointMgr) {
        size_t count = checkpointMgr->clearOldCheckpoints(0);  // 清理所有检查点
        std::cout << "已清理 " << count << " 个旧检查点\n";
    }

    std::cout << "\n========================================\n";
    std::cout << "  所有测试完成！\n";
    std::cout << "========================================\n";

    return 0;
}
