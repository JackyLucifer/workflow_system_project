/**
 * @file version_control_example.cpp
 * @brief 业务流程管理系统 - 版本控制功能示例
 *
 * 演示功能：
 * - 保存工作流版本
 * - 获取版本历史
 * - 回滚到指定版本
 * - 标记激活版本
 * - 删除版本
 */

#include "workflow_system/implementations/workflow_version_control_impl.h"
#include "workflow_system/implementations/sqlite_workflow_persistence.h"
#include "workflow_system/core/logger.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;

// 辅助函数：打印版本信息
void printVersionInfo(const WorkflowVersion& version) {
    std::cout << "========================================" << std::endl;
    std::cout << "版本 ID: " << version.versionId << std::endl;
    std::cout << "工作流名称: " << version.workflowName << std::endl;
    std::cout << "作者: " << version.author << std::endl;
    std::cout << "描述: " << version.description << std::endl;
    std::cout << "创建时间: " << version.createdAt << std::endl;
    std::cout << "父版本: " << version.parentVersion << std::endl;
    std::cout << "是否激活: " << (version.isActive ? "是" : "否") << std::endl;
    std::cout << "========================================" << std::endl;
}

int main() {
    std::cout << "\n========== 工作流版本控制示例 ==========\n" << std::endl;

    // 初始化持久化（用于存储版本信息）
    auto& persistence = SqliteWorkflowPersistence::getInstance();
    std::string dbPath = "workflows.db";
    if (!persistence.initialize(dbPath)) {
        std::cerr << "数据库�失败！" << std::endl;
        return 1;
    }

    // 创建版本控制器
    WorkflowVersionControl versionControl;

    std::string workflowName = "数据处理流程";

    // 示例工作流定义（JSON）
    std::string v1Definition = R"({
        "name": "数据处理流程 v1",
        "nodes": [
            {"id": "node1", "name": "读取数据"},
            {"id": "node2", "name": "处理数据"}
        ],
        "edges": [
            {"from": "node1", "to": "node2"}
        ]
    })";

    std::string v2Definition = R"({
        "name": "数据处理流程 v2",
        "nodes": [
            {"id": "node1", "name": "读取数据"},
            {"id": "node2", "name": "转换数据"},
            {"id": "node3", "name": "保存数据"}
        ],
        "edges": [
            {"from": "node1", "to": "node2"},
            {"from": "node2", "to": "node3"}
        ]
    })";

    std::string v3Definition = R"({
        "name": "数据处理流程 v3",
        "nodes": [
            {"id": "node1", "name": "读取数据"},
            {"id": "node2", "name": "验证数据"},
            {"id": "node3", "name": "转换数据"},
            {"id": "node4", "name": "保存数据"}
        ],
        "edges": [
            {"from": "node1", "to": "node2"},
            {"from": "node2", "to": "node3"},
            {"from": "node3", "to": "node4"}
        ]
    })";

    // ========== 保存版本 ==========
    std::cout << "\n[1] 保存多个版本..." << std::endl;

    std::string v1Id = versionControl.saveVersion(workflowName, v1Definition, "系统管理员", "初始版本");
    std::cout << "保存版本 v1: " << v1Id << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string v2Id = versionControl.saveVersion(workflowName, v2Definition, "开发人员 A", "增加数据转换节点");
    std::cout << "保存版本 v2: " << v2Id << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string v3Id = versionControl.saveVersion(workflowName, v3Definition, "开发人员 B", "增加数据验证和优化处理");
    std::cout << "保存版本 v3: " << v3Id << std::endl;

    // ========== 获取所有版本 ==========
    std::cout << "\n[2] 获取所有版本..." << std::endl;
    auto versions = versionControl.getAllVersions(workflowName);
    std::cout << "找到 " << versions.size() << " 个版本" << std::endl;
    for (const auto& version : versions) {
        printVersionInfo(version);
    }

    // ========== 获取最新版本 ==========
    std::cout << "\n[3] 获取最新版本..." << std::endl;
    auto latest = versionControl.getLatestVersion(workflowName);
    printVersionInfo(latest);

    // ========== 回滚测试 ==========
    std::cout << "\n[4] 回滚到 v1..." << std::endl;
    if (versionControl.rollbackToVersion(workflowName, v1Id)) {
        std::cout << "回滚成功" << std::endl;
        auto current = versionControl.getLatestVersion(workflowName);
        printVersionInfo(current);
    } else {
        std::cout << "回滚失败" << std::endl;
    }

    // ========== 标记激活版本 ==========
    std::cout << "\n[5] 标记 v2 为激活版本..." << std::endl;
    if (versionControl.setActiveVersion(workflowName, v2Id)) {
        std::cout << "设置成功" << std::endl;
        versions = versionControl.getAllVersions(workflowName);
        for (const auto& version : versions) {
            std::cout << "  " << version.versionId << ": "
                      << (version.isActive ? "激活" : "非激活") << std::endl;
        }
    } else {
        std::cout << "设置失败" << std::endl;
    }

    // ========== 比较版本差异 ==========
    std::cout << "\n[6] 比较 v1 和 v2 的差异..." << std::endl;
    auto diff = versionControl.compareVersions(workflowName, v1Id, v2Id);
    if (!diff.versionFrom.empty() && !diff.versionTo.empty()) {
        std::cout << "差异对比：" << diff.versionFrom << " -> " << diff.versionTo << std::endl;
        std::cout << "摘要: " << diff.summary << std::endl;
        std::cout << "修改的节点数量: " << diff.changedNodes.size() << std::endl;
    }

    // ========== 删除版本测试 ==========
    std::cout << "\n[7] 删除 v3..." << std::endl;
    if (versionControl.deleteVersion(workflowName, v3Id)) {
        std::cout << "删除成功" << std::endl;
        versions = versionControl.getAllVersions(workflowName);
        std::cout << "剩余 " << versions.size() << " 个版本" << std::endl;
    } else {
        std::cout << "删除失败" << std::endl;
    }

    // ========== 获取所有工作流 ==========
    std::cout << "\n[8] 获取所有工作流名称..." << std::endl;
    auto workflows = versionControl.getAllWorkflowNames();
    std::cout << "找到 " << workflows.size() << " 个工作流:" << std::endl;
    for (const auto& name : workflows) {
        std::cout << "  - " << name << std::endl;
    }

    // ========== 清理 ==========
    std::cout << "\n[9] 关闭数据库..." << std::endl;
    persistence.close();

    std::cout << "\n========== 示例完成 ==========" << std::endl;
    return 0;
}
