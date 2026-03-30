/**
 * @file conditional_workflow.h
 * @brief 业务流程管理系统 - 条件分支工作流接口
 *
 * 设计模式：
 * - 策略模式：根据条件选择不同的执行路径
 * - 组合模式：可以组合多个工作流作为分支
 */

#ifndef WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_H
#define WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "workflow_system/core/types.h"
#include "workflow_system/core/any.h"
#include "workflow_system/interfaces/workflow.h"

namespace WorkflowSystem {

/**
 * @brief 条件分支定义
 */
struct Branch {
    std::string name;           // 分支名称
    std::string conditionName;   // 条件名称
    std::function<bool()> condition; // 条件函数，返回 true 时选择此分支
    std::shared_ptr<IWorkflow> workflow; // 分支对应的工作流
    int priority;                  // 优先级（多个条件满足时使用）
    std::string description;      // 分支描述

    // 默认构造函数
    Branch() : priority(0) {}
};

/**
 * @brief 条件分支工作流接口
 *
 * 功能：
 * - 根据条件选择执行不同的工作流
 * - 支持多个分支和优先级
 * - 支持默认分支（所有条件都不满足时）
 */
class IConditionalWorkflow {
public:
    virtual ~IConditionalWorkflow() = default;

    /**
     * @brief 添加分支
     * @param branch 分支定义
     */
    virtual void addBranch(const Branch& branch) = 0;

    /**
     * @brief 设置默认分支（所有条件都不满足时执行）
     * @param workflow 默认工作流
     */
    virtual void setDefaultBranch(std::shared_ptr<IWorkflow> workflow) = 0;

    /**
     * @brief 设置默认分支
     * @param branch 分支定义
     */
    virtual void setDefaultBranch(const Branch& branch) = 0;

    /**
     * @brief 获取选中的分支
     * @return 选中的分支，如果没有选中返回 nullptr
     */
    virtual Branch* getSelectedBranch() = 0;

    /**
     * @brief 获取所有分支
     * @return 分支列表
     */
    virtual std::vector<Branch> getAllBranches() const = 0;

    /**
     * @brief 获取默认分支
     * @return 默认分支
     */
    virtual Branch* getDefaultBranch() = 0;

    /**
     * @brief 清空所有分支
     */
    virtual void clearBranches() = 0;

    /**
     * @brief 执行条件评估并选择分支
     * @return 是否成功选择并启动了分支
     */
    virtual bool evaluateAndExecute() = 0;
};

/**
 * @brief 条件评估器接口
 *
 * 用于自定义条件评估逻辑
 */
class IConditionEvaluator {
public:
    virtual ~IConditionEvaluator() = default;

    /**
     * @brief 评估条件
     * @param conditionName 条件名称
     * @param context 工作流上下文
     * @return 条件是否满足
     */
    virtual bool evaluate(const std::string& conditionName,
                     std::shared_ptr<IWorkflowContext> context) = 0;

    /**
     * @brief 注册条件函数
     * @param name 条件名称
     * @param condition 条件函数
     */
    virtual void registerCondition(const std::string& name,
                               std::function<bool(std::shared_ptr<IWorkflowContext>)> condition) = 0;

    /**
     * @brief 移除条件
     * @param name 条件名称
     */
    virtual void removeCondition(const std::string& name) = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CONDITIONAL_WORKFLOW_H
