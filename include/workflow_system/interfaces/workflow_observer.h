/**
 * @file workflow_observer.h
 * @brief 业务流程管理系统 - 工作流观察者接口
 *
 * 设计模式：观察者模式
 * 当工作流状态变化时通知观察者
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_OBSERVER_H
#define WORKFLOW_SYSTEM_WORKFLOW_OBSERVER_H

#include <string>

namespace WorkflowSystem {

/**
 * @brief 工作流观察者接口
 *
 * 设计模式：观察者模式
 * 定义观察者需要实现的方法
 */
class IWorkflowObserver {
public:
    virtual ~IWorkflowObserver() = default;

    virtual void onWorkflowStarted(const std::string& workflowName) = 0;
    virtual void onWorkflowFinished(const std::string& workflowName) = 0;
    virtual void onWorkflowInterrupted(const std::string& workflowName) = 0;
    virtual void onWorkflowError(const std::string& workflowName, const std::string& error) = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_OBSERVER_H
