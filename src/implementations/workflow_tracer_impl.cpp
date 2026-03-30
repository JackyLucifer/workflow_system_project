/**
 * @file workflow_tracer_impl.cpp
 * @brief 工作流追踪器实现
 */

#include "workflow_system/implementations/workflow_tracer_impl.h"

namespace WorkflowSystem {

// ========== WorkflowTracer 实现 ==========

WorkflowTracer::WorkflowTracer() : maxTraces_(10000) {}

WorkflowTracer::~WorkflowTracer() = default;

std::string WorkflowTracer::startTrace(const std::string& workflowName) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string traceId = "trace_" + IdGenerator::generate();

    ExecutionTrace trace;
    trace.traceId = traceId;
    trace.workflowName = workflowName;
    trace.startTime = TimeUtils::getCurrentMs();
    trace.finalState = WorkflowState::RUNNING;

    activeTraces_[traceId] = trace;
    workflowTraces_[workflowName].push_back(traceId);

    LOG_INFO("[WorkflowTracer] Started trace: " + traceId + " for workflow: " + workflowName);

    return traceId;
}

void WorkflowTracer::endTrace(const std::string& traceId, WorkflowState finalState) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeTraces_.find(traceId);
    if (it == activeTraces_.end()) {
        return;
    }

    it->second.endTime = TimeUtils::getCurrentMs();
    it->second.totalDuration = it->second.endTime - it->second.startTime;
    it->second.finalState = finalState;

    completedTraces_.push_back(it->second);
    activeTraces_.erase(it);

    if (completedTraces_.size() > static_cast<size_t>(maxTraces_)) {
        completedTraces_.erase(completedTraces_.begin());
    }

    LOG_INFO("[WorkflowTracer] Ended trace: " + traceId +
             " with state: " + workflowStateToString(finalState) +
             " duration: " + std::to_string(it->second.totalDuration) + "ms");
}

void WorkflowTracer::recordEvent(const std::string& traceId, const TraceEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeTraces_.find(traceId);
    if (it != activeTraces_.end()) {
        it->second.events.push_back(event);
        it->second.eventCount++;
        if (event.type == TraceEventType::ERROR_OCCURRED) {
            it->second.errorCount++;
        }
        if (event.type == TraceEventType::RETRY_ATTEMPT) {
            it->second.retryCount++;
        }
    }
}

void WorkflowTracer::recordStateChange(const std::string& traceId,
                                       WorkflowState fromState,
                                       WorkflowState toState) {
    TraceEvent event;
    event.traceId = traceId;
    event.type = TraceEventType::STATE_CHANGED;
    event.timestamp = TimeUtils::getCurrentMs();
    event.fromState = fromState;
    event.toState = toState;
    event.message = "State changed from " + workflowStateToString(fromState) +
                   " to " + workflowStateToString(toState);

    recordEvent(traceId, event);
}

void WorkflowTracer::recordError(const std::string& traceId, const std::string& errorMessage) {
    TraceEvent event;
    event.traceId = traceId;
    event.type = TraceEventType::ERROR_OCCURRED;
    event.timestamp = TimeUtils::getCurrentMs();
    event.error = errorMessage;
    event.message = "Error occurred: " + errorMessage;

    recordEvent(traceId, event);
}

void WorkflowTracer::recordNodeExecution(const std::string& traceId,
                                         const std::string& nodeId,
                                         bool success,
                                         int64_t duration) {
    TraceEvent event;
    event.traceId = traceId;
    event.nodeId = nodeId;
    event.type = success ? TraceEventType::NODE_COMPLETED : TraceEventType::NODE_FAILED;
    event.timestamp = TimeUtils::getCurrentMs();
    event.duration = duration;
    event.message = "Node " + nodeId + (success ? " completed" : " failed");

    recordEvent(traceId, event);
}

ExecutionTrace WorkflowTracer::getTrace(const std::string& traceId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = activeTraces_.find(traceId);
    if (it != activeTraces_.end()) {
        return it->second;
    }

    for (const auto& trace : completedTraces_) {
        if (trace.traceId == traceId) {
            return trace;
        }
    }

    return ExecutionTrace();
}

std::vector<ExecutionTrace> WorkflowTracer::getTracesByWorkflow(const std::string& workflowName, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ExecutionTrace> result;

    auto wit = workflowTraces_.find(workflowName);
    if (wit == workflowTraces_.end()) {
        return result;
    }

    for (auto rit = wit->second.rbegin();
         rit != wit->second.rend() && static_cast<int>(result.size()) < limit;
         ++rit) {
        auto trace = getTraceInternal(*rit);
        if (!trace.traceId.empty()) {
            result.push_back(trace);
        }
    }

    return result;
}

std::vector<ExecutionTrace> WorkflowTracer::getActiveTraces() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ExecutionTrace> result;
    for (const auto& pair : activeTraces_) {
        result.push_back(pair.second);
    }
    return result;
}

TraceStatistics WorkflowTracer::getStatistics(const std::string& workflowName) const {
    std::lock_guard<std::mutex> lock(mutex_);

    TraceStatistics stats;
    stats.workflowName = workflowName;

    auto traces = getTracesByWorkflowInternal(workflowName);

    for (const auto& trace : traces) {
        stats.totalExecutions++;

        if (trace.finalState == WorkflowState::COMPLETED) {
            stats.successCount++;
        } else {
            stats.failureCount++;
        }

        stats.totalDuration += trace.totalDuration;

        if (stats.minDuration == 0 || trace.totalDuration < stats.minDuration) {
            stats.minDuration = trace.totalDuration;
        }
        if (trace.totalDuration > stats.maxDuration) {
            stats.maxDuration = trace.totalDuration;
        }
    }

    if (stats.totalExecutions > 0) {
        stats.avgDuration = stats.totalDuration / stats.totalExecutions;
        stats.successRate = static_cast<double>(stats.successCount) / stats.totalExecutions * 100.0;
    }

    return stats;
}

void WorkflowTracer::clearTraces(const std::string& workflowName) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (workflowName.empty()) {
        activeTraces_.clear();
        completedTraces_.clear();
        workflowTraces_.clear();
    } else {
        auto wit = workflowTraces_.find(workflowName);
        if (wit != workflowTraces_.end()) {
            for (const auto& traceId : wit->second) {
                activeTraces_.erase(traceId);
            }
            workflowTraces_.erase(wit);
        }

        completedTraces_.erase(
            std::remove_if(completedTraces_.begin(), completedTraces_.end(),
                [&workflowName](const ExecutionTrace& t) {
                    return t.workflowName == workflowName;
                }),
            completedTraces_.end());
    }
}

void WorkflowTracer::setMaxTraces(int maxTraces) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxTraces_ = maxTraces;
}

std::string WorkflowTracer::exportTraces(const std::string& format) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string result;

    if (format == "json") {
        result = "[\n";
        bool first = true;
        for (const auto& trace : completedTraces_) {
            if (!first) result += ",\n";
            first = false;
            result += "  {\"traceId\":\"" + trace.traceId + "\",";
            result += "\"workflowName\":\"" + trace.workflowName + "\",";
            result += "\"duration\":" + std::to_string(trace.totalDuration) + ",";
            result += "\"state\":\"" + workflowStateToString(trace.finalState) + "\"}";
        }
        result += "\n]";
    } else {
        result = "traceId,workflowName,duration,state\n";
        for (const auto& trace : completedTraces_) {
            result += trace.traceId + "," + trace.workflowName + "," +
                     std::to_string(trace.totalDuration) + "," +
                     workflowStateToString(trace.finalState) + "\n";
        }
    }

    return result;
}

// ========== 私有辅助方法 ==========

ExecutionTrace WorkflowTracer::getTraceInternal(const std::string& traceId) const {
    auto it = activeTraces_.find(traceId);
    if (it != activeTraces_.end()) {
        return it->second;
    }

    for (const auto& trace : completedTraces_) {
        if (trace.traceId == traceId) {
            return trace;
        }
    }

    return ExecutionTrace();
}

std::vector<ExecutionTrace> WorkflowTracer::getTracesByWorkflowInternal(const std::string& workflowName) const {
    std::vector<ExecutionTrace> result;

    for (const auto& trace : completedTraces_) {
        if (workflowName.empty() || trace.workflowName == workflowName) {
            result.push_back(trace);
        }
    }

    return result;
}

} // namespace WorkflowSystem
