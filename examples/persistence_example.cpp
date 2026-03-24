/**
 * @file persistence_example.cpp
 * @brief 业务流程管理系统 - SQLite 持久化功能示例
 *
 * 演示功能：
 * - 初始化 SQLite 数据库
 * - 保存工作流记录
 * - 查询工作流记录（按 ID、名称、状态、时间范围）
 * - 统计信息（总数、成功率、平均耗时）
 * - 保存和获取工作流定义
 * - 删除和清空工作流记录
 */

#include "implementations/sqlite_workflow_persistence.h"
#include "interfaces/workflow_persistence.h"
#include "core/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <random>

using namespace WorkflowSystem;

// 辅助函数：打印工作流记录
void printWorkflowRecord(const WorkflowRecord& record) {
    std::cout << "========================================" << std::endl;
    std::cout << "工作流 ID: " << record.workflowId << std::endl;
    std::cout << "工作流名称: " << record.workflowName << std::endl;
    std::cout << "状态: ";

    switch (record.state) {
        case WorkflowState::PENDING: std::cout << "PENDING"; break;
        case WorkflowState::RUNNING: std::cout << "RUNNING"; break;
        case WorkflowState::PAUSED: std::cout << "PAUSED"; break;
        case WorkflowState::COMPLETED: std::cout << "COMPLETED"; break;
        case WorkflowState::FAILED: std::cout << "FAILED"; break;
        case WorkflowState::ABORTED: std::cout << "ABORTED"; break;
    }
    std::cout << std::endl;

    std::cout << "开始时间: " << record.startTime << std::endl;
    std::cout << "结束时间: " << record.endTime << std::endl;
    std::cout << "执行耗时: " << record.duration << "ms" << std::endl;
    std::cout << "成功: " << (record.success ? "是" : "否") << std::endl;

    if (!record.errorMessage.empty()) {
        std::cout << "错误信息: " << record.errorMessage << std::endl;
    }

    std::cout << "重试次数: " << record.retryCount << std::endl;
    std::cout << "========================================" << std::endl;
}

// 辅助函数：创建示例工作流记录
WorkflowRecord createSampleWorkflow(const std::string& name, WorkflowState state,
                                    bool success, const std::string& error = "") {
    WorkflowRecord record;
    record.workflowName = name;
    record.state = state;
    record.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    record.endTime = record.startTime + 1000; // 假设执行 1 秒
    record.duration = 1000;
    record.success = success;
    record.errorMessage = error;
    record.retryCount = 0;

    // 添加一些变量
    record.variables["task"] = "sample_task";
    record.variables["priority"] = "high";
    record.variables["owner"] = "user1";

    return record;
}

int main() {
    std::cout << "\n========== SQLite 持久化功能示例 ==========\n" << std::endl;

    // 获取单例实例
    auto& persistence = SqliteWorkflowPersistence::getInstance();

    // 初始化数据库
    std::string dbPath = "workflows.db";
    std::cout << "[1] 初始化数据库: " << dbPath << std::endl;
    if (!persistence.initialize(dbPath)) {
        std::cerr << "数据库初始化失败！" << std::endl;
        return 1;
    }
    std::cout << "数据库初始化成功！" << std::endl;

    // 清空旧数据（如果存在）
    std::cout << "\n[2] 清空旧数据..." << std::endl;
    persistence.clearAllWorkflows();
    std::cout << "旧数据已清空" << std::endl;

    // 创建并保存多个工作流记录
    std::cout << "\n[3] 创建并保存工作流记录..." << std::endl;
    std::vector<std::string> workflowIds;

    // 辅助函数：生成工作流ID
    auto generateWorkflowId = []() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<> dis(0, 9999);
        return "wf_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    };

    // 保存成功的工作流
    auto wf1 = createSampleWorkflow("数据处理工作流", WorkflowState::COMPLETED, true);
    wf1.workflowId = generateWorkflowId();
    if (persistence.saveWorkflow(wf1)) {
        workflowIds.push_back(wf1.workflowId);
        std::cout << "保存工作流 1: " << wf1.workflowId << std::endl;
    } else {
        std::cerr << "保存工作流 1 失败" << std::endl;
    }

    // 保存失败的工作流
    auto wf2 = createSampleWorkflow("数据分析工作流", WorkflowState::FAILED, false,
                                     "连接数据库超时");
    wf2.duration = 2000; // 更长的执行时间
    wf2.workflowId = generateWorkflowId();
    if (persistence.saveWorkflow(wf2)) {
        workflowIds.push_back(wf2.workflowId);
        std::cout << "保存工作流 2: " << wf2.workflowId << std::endl;
    } else {
        std::cerr << "保存工作流 2 失败" << std::endl;
    }

    // 保存运行中的工作流
    auto wf3 = createSampleWorkflow("文件上传工作流", WorkflowState::RUNNING, false);
    wf3.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    wf3.endTime = wf3.startTime + 5000;
    wf3.duration = 5000;
    wf3.workflowId = generateWorkflowId();
    if (persistence.saveWorkflow(wf3)) {
        workflowIds.push_back(wf3.workflowId);
        std::cout << "保存工作流 3: " << wf3.workflowId << std::endl;
    } else {
        std::cerr << "保存工作流 3 失败" << std::endl;
    }

    // 再保存几个工作流用于统计测试
    for (int i = 0; i < 7; ++i) {
        auto wf = createSampleWorkflow("批量工作流 " + std::to_string(i + 1),
                                       WorkflowState::COMPLETED, true);
        wf.duration = 500 + (i * 100);
        wf.workflowId = generateWorkflowId();
        if (persistence.saveWorkflow(wf)) {
            workflowIds.push_back(wf.workflowId);
        }
    }

    std::cout << "总共保存了 " << workflowIds.size() << " 个工作流" << std::endl;

    // 根据 ID 获取工作流
    if (!workflowIds.empty()) {
        std::cout << "\n[4] 根据 ID 获取工作流: " << workflowIds[0] << std::endl;
        auto retrieved = persistence.getWorkflowById(workflowIds[0]);
        printWorkflowRecord(retrieved);
    }

    // 获取所有工作流
    std::cout << "\n[5] 获取所有工作流..." << std::endl;
    auto allWorkflows = persistence.getAllWorkflows();
    std::cout << "总共 " << allWorkflows.size() << " 个工作流" << std::endl;
    for (const auto& wf : allWorkflows) {
        std::cout << "  - " << wf.workflowId << " (" << wf.workflowName << ")"
                  << " [" << (wf.success ? "成功" : "失败") << "]" << std::endl;
    }

    // 根据名称查询（模糊匹配）
    std::cout << "\n[6] 根据名称查询 '数据'..." << std::endl;
    auto dataWorkflows = persistence.getWorkflowsByName("数据");
    std::cout << "找到 " << dataWorkflows.size() << " 个匹配的工作流" << std::endl;
    for (const auto& wf : dataWorkflows) {
        printWorkflowRecord(wf);
    }

    // 根据状态查询
    std::cout << "\n[7] 根据状态查询 COMPLETED..." << std::endl;
    auto completedWorkflows = persistence.getWorkflowsByState(WorkflowState::COMPLETED);
    std::cout << "找到 " << completedWorkflows.size() << " 个已完成的工作流" << std::endl;

    // 统计信息
    std::cout << "\n[8] 统计信息..." << std::endl;
    std::cout << "总工作流数: " << persistence.getTotalWorkflowCount() << std::endl;
    std::cout << "成功数: " << persistence.getSuccessCount() << std::endl;
    std::cout << "失败数: " << persistence.getFailureCount() << std::endl;
    std::cout << "成功率: " << std::fixed << std::setprecision(2)
              << (persistence.getSuccessRate() * 100) << "%" << std::endl;
    std::cout << "平均耗时: " << persistence.getAverageDuration() << "ms" << std::endl;

    // 保存工作流定义
    std::cout << "\n[9] 保存工作流定义..." << std::endl;
    std::string workflowDef = R"({
        "name": "数据处理流水线",
        "description": "处理输入数据并生成报告",
        "nodes": [
            {"id": "node1", "name": "数据读取", "type": "reader"},
            {"id": "node2", "name": "数据转换", "type": "transformer"},
            {"id": "node3", "name": "数据保存", "type": "writer"}
        ],
        "edges": [
            {"from": "node1", "to": "node2"},
            {"from": "node2", "to": "node3"}
        ]
    })";

    std::string defId = "workflow_def_001";
    if (persistence.saveWorkflowDefinition(defId, workflowDef)) {
        std::cout << "工作流定义已保存，ID: " << defId << std::endl;
    }

    // 获取工作流定义
    std::cout << "\n[10] 获取工作流定义..." << std::endl;
    std::string retrievedDef = persistence.getWorkflowDefinition(defId);
    if (!retrievedDef.empty()) {
        std::cout << "工作流定义内容:\n" << retrievedDef << std::endl;
    }

    // 获取所有工作流定义
    std::cout << "\n[11] 获取所有工作流定义..." << std::endl;
    auto allDefs = persistence.getAllWorkflowDefinitions();
    std::cout << "找到 " << allDefs.size() << " 个工作流定义" << std::endl;
    for (const auto& pair : allDefs) {
        std::cout << "  - " << pair.first << std::endl;
    }

    // 删除工作流
    if (workflowIds.size() >= 2) {
        std::cout << "\n[12] 删除工作流: " << workflowIds[1] << std::endl;
        if (persistence.deleteWorkflow(workflowIds[1])) {
            std::cout << "工作流删除成功" << std::endl;
            std::cout << "当前工作流数: " << persistence.getTotalWorkflowCount() << std::endl;
        }
    }

    // 关闭数据库
    std::cout << "\n[13] 关闭数据库..." << std::endl;
    persistence.close();
    std::cout << "数据库已关闭" << std::endl;

    std::cout << "\n========== 示例完成 ==========" << std::endl;
    std::cout << "数据库文件: " << dbPath << std::endl;
    std::cout << "您可以使用 SQLite 客户端工具查看数据" << std::endl;

    return 0;
}
