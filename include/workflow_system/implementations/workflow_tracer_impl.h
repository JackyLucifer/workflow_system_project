/**
 * @file workflow_tracer_impl.h
 * @brief 工作流追踪器实现
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_TRACER_IMPL_H
#define WORKFLOW_SYSTEM_WORKFLOW_TRACER_IMPL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include "workflow_system/interfaces/workflow_tracer.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"

namespace WorkflowSystem {

/**
 * @brief 工作流追踪器实现
 */
class WorkflowTracer : public IWorkflowTracer {
public:
    WorkflowTracer();

    ~WorkflowTracer() override;

    std::string startTrace(const std::string& workflowName) override;

    void endTrace(const std::string& traceId, WorkflowState finalState) override;

    void recordEvent(const std::string& traceId, const TraceEvent& event) override;

    void recordStateChange(const std::string& traceId,
                          WorkflowState fromState,
                          WorkflowState toState) override;

    void recordError(const std::string& traceId, const std::string& errorMessage) override;

    void recordNodeExecution(const std::string& traceId,
                            const std::string& nodeId,
                            bool success,
                            int64_t duration) override;

    ExecutionTrace getTrace(const std::string& traceId) const override;

    std::vector<ExecutionTrace> getTracesByWorkflow(const std::string& workflowName, int limit) const override;

    std::vector<ExecutionTrace> getActiveTraces() const override;

    TraceStatistics getStatistics(const std::string& workflowName) const override;

    void clearTraces(const std::string& workflowName) override;

    void setMaxTraces(int maxTraces) override;

    std::string exportTraces(const std::string& format) const override;

private:
    mutable std::mutex mutex_;
    int maxTraces_;

    std::unordered_map<std::string, ExecutionTrace> activeTraces_;
    std::vector<ExecutionTrace> completedTraces_;
    std::unordered_map<std::string, std::vector<std::string>> workflowTraces_;

    ExecutionTrace getTraceInternal(const std::string& traceId) const;

    std::vector<ExecutionTrace> getTracesByWorkflowInternal(const std::string& workflowName) const;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_TRACER_IMPL_H
