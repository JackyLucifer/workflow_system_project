/**
 * @file scheduler_example.cpp
 * @brief 工作流调度器示例
 */

#include "workflow_system/implementations/workflow_scheduler_impl.h"
#include "workflow_system/implementations/workflow_manager.h"
#include "workflow_system/implementations/workflows/base_workflow.h"
#include "workflow_system/core/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

// 简单测试工作流
class TestWorkflow : public BaseWorkflow {
public:
    TestWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("TestWorkflow", resourceManager), executed_(false) {}
    
    bool wasExecuted() const { return executed_; }
    
protected:
    Any onExecute() override {
        executed_ = true;
        LOG_INFO("[TestWorkflow] Executed at: " + std::to_string(TimeUtils::getCurrentMs()));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        return Any(42);
    }
    
private:
    std::atomic<bool> executed_;
};

int main() {
    std::cout << "\n========== 工作流调度器示例 ==========\n" << std::endl;
    
    auto& logger = Logger::getInstance();
    logger.setLevel(LogLevel::INFO);
    
    // 创建调度器
    WorkflowScheduler scheduler;
    
    // 设置执行回调
    scheduler.setExecutionCallback([](const ScheduleRecord& record) {
        std::cout << "执行调度: " << record.scheduleId 
                  << " 状态: " << (record.state == ScheduleState::COMPLETED ? "完成" : "失败")
                  << " 时间: " << std::to_string(record.actualTime) << std::endl;
    });
    
    // 示例1：一次性执行
    ScheduleConfig onceConfig;
    onceConfig.workflowName = "TestWorkflow";
    onceConfig.type = ScheduleType::ONCE;
    onceConfig.startTime = TimeUtils::getCurrentMs() + 1000;
    onceConfig.description = "一次性执行测试";
    
    std::string onceId = scheduler.schedule(onceConfig);
    std::cout << "已调度一次性任务: " << onceId << std::endl;
    
    // 示例2：固定间隔执行
    ScheduleConfig intervalConfig;
    intervalConfig.workflowName = "TestWorkflow";
    intervalConfig.type = ScheduleType::INTERVAL;
    intervalConfig.intervalMs = 2000;
    intervalConfig.maxExecutions = 3;
    intervalConfig.description = "间隔执行测试（3次）";
    
    std::string intervalId = scheduler.schedule(intervalConfig);
    std::cout << "已调度间隔任务: " << intervalId << std::endl;
    
    // 启动调度器
    scheduler.start();
    std::cout << "调度器已启动\n" << std::endl;
    
    // 查询下次执行时间
    auto nextTimes = scheduler.getAllNextExecutionTimes();
    std::cout << "\n下次执行时间:\n";
    for (const auto& pair : nextTimes) {
        std::cout << "  " << pair.first << ": " 
                  << TimeUtils::formatDuration(pair.second - TimeUtils::getCurrentMs()) 
                  << " 后执行" << std::endl;
    }
    
    // 等待执行
    std::cout << "\n等待执行... (按 Ctrl+C 退出)\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 查询调度记录
    auto records = scheduler.getScheduleRecords(intervalId, 10);
    std::cout << "\n调度记录 (" << records.size() << " 条):\n";
    for (const auto& record : records) {
        std::cout << "  执行#" << record.executionCount 
                  << " 状态: " << (record.state == ScheduleState::COMPLETED ? "完成" : "失败")
                  << std::endl;
    }
    
    // 停止调度器
    scheduler.stop();
    std::cout << "\n调度器已停止\n" << std::endl;
    
    std::cout << "\n========== 示例完成 ==========\n" << std::endl;
    return 0;
}
