/**
 * @file workflow_persistence.h
 * @brief 业务流程管理系统 - 工作流持久化接口
 *
 * 设计模式：
 * - 单例模式：全局唯一的持久化管理器
 * - 工厂模式：创建持久化管理器
 *
 * 面向对象：
 * - 封装：封装工作流数据的存储和检索
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_PERSISTENCE_H
#define WORKFLOW_SYSTEM_WORKFLOW_PERSISTENCE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "workflow_system/core/types.h"

namespace WorkflowSystem {

/**
 * @brief 工作流记录
 */
struct WorkflowRecord {
    std::string workflowId;          // 工作流ID
    std::string workflowName;        // 工作流名称
    WorkflowState state;            // 工作流状态
    int64_t startTime;             // 开始时间戳（毫秒）
    int64_t endTime;               // 结束时间戳（毫秒）
    int64_t duration;               // 执行耗时（毫秒）
    bool success;                  // 是否成功
    std::string errorMessage;       // 错误信息
    std::map<std::string, std::string> variables;  // 工作流变量
    int retryCount;               // 重试次数
    std::string inputConfig;       // 输入配置（如果有）
};

/**
 * @brief 工作流持久化接口
 */
class IWorkflowPersistence {
public:
    virtual ~IWorkflowPersistence() = default;

    /**
     * @brief 初始化持久化存储
     * @return true 初始化成功，false 失败
     */
    virtual bool initialize(const std::string& databasePath) = 0;

    /**
     * @brief 关闭持久化存储
     */
    virtual void close() = 0;

    /**
     * @brief 保存工作流记录
     * @param record 工作流记录
     * @return true 保存成功，false 失败
     */
    virtual bool saveWorkflow(const WorkflowRecord& record) = 0;

    /**
     * @brief 根据ID获取工作流记录
     * @param workflowId 工作流ID
     * @return 工作流记录，如果不存在返回空记录
     */
    virtual WorkflowRecord getWorkflowById(const std::string& workflowId) const = 0;

    /**
     * @brief 获取所有工作流记录
     * @return 工作流记录列表
     */
    virtual std::vector<WorkflowRecord> getAllWorkflows() const = 0;

    /**
     * @brief 根据名称查询工作流记录
     * @param name 工作流名称（支持模糊匹配）
     * @return 匹配的工作流记录列表
     */
    virtual std::vector<WorkflowRecord> getWorkflowsByName(const std::string& name) const = 0;

    /**
     * @brief 根据状态查询工作流记录
     * @param state 工作流状态
     * @return 符合条件的工作流记录列表
     */
    virtual std::vector<WorkflowRecord> getWorkflowsByState(WorkflowState state) const = 0;

    /**
     * @brief 根据时间范围查询工作流记录
     * @param startTime 开始时间戳（毫秒）
     * @param endTime 结束时间戳（毫秒）
     * @return 时间范围内的工作流记录列表
     */
    virtual std::vector<WorkflowRecord> getWorkflowsByTimeRange(int64_t startTime, int64_t endTime) const = 0;

    /**
     * @brief 删除工作流记录
     * @param workflowId 工作流ID
     * @return true 删除成功，false 失败
     */
    virtual bool deleteWorkflow(const std::string& workflowId) = 0;

    /**
     * @brief 清除所有工作流记录
     * @return true 清除成功，false 失败
     */
    virtual bool clearAllWorkflows() = 0;

    /**
     * @brief 获取工作流执行统计
     * @return 总执行次数
     */
    virtual int getTotalWorkflowCount() const = 0;

    /**
     * @brief 获取成功的工作流记录数量
     * @return 成功数量
     */
    virtual int getSuccessCount() const = 0;

    /**
     * @brief 获取失败的工作流记录数量
     * @return 失败数量
     */
    virtual int getFailureCount() const = 0;

    /**
     * @brief 计算成功率
     * @return 成功率（0-1 之间的浮点数）
     */
    virtual double getSuccessRate() const = 0;

    /**
     * @brief 获取平均执行时间
     * @return 平均耗时（毫秒）
     */
    virtual int64_t getAverageDuration() const = 0;

    /**
     * @brief 保存工作流定义（JSON格式）
     * @param workflowId 工作流ID
     * @param workflowJson 工作流定义JSON
     * @return true 保存成功，false 失败
     */
    virtual bool saveWorkflowDefinition(const std::string& workflowId,
                                     const std::string& workflowJson) = 0;

    /**
     * @brief 获取工作流定义
     * @param workflowId 工作流ID
     * @return 工作流定义JSON，如果不存在返回空字符串
     */
    virtual std::string getWorkflowDefinition(const std::string& workflowId) const = 0;

    /**
     * @brief 获取所有工作流定义
     * @return 工作流定义映射（ID -> JSON）
     */
    virtual std::map<std::string, std::string> getAllWorkflowDefinitions() const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_PERSISTENCE_H
