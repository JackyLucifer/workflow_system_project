/**
 * @file checkpoint_manager_impl.h
 * @brief 检查点管理器实现 - 断点续传支持
 */

#ifndef WORKFLOW_SYSTEM_CHECKPOINT_MANAGER_IMPL_H
#define WORKFLOW_SYSTEM_CHECKPOINT_MANAGER_IMPL_H

#include "workflow_system/interfaces/checkpoint_manager.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"
#include <unordered_map>
#include <deque>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace WorkflowSystem {

/**
 * @brief 检查点管理器实现
 */
class CheckpointManager : public ICheckpointManager {
public:
    CheckpointManager() 
        : config_()
        , logger_(Logger::getInstance()) {}
    
    explicit CheckpointManager(const CheckpointConfig& config)
        : config_(config)
        , logger_(Logger::getInstance()) {}
    
    // ========== 检查点创建 ==========
    
    std::string createCheckpoint(
        const std::string& workflowName,
        const std::string& executionId,
        CheckpointType type,
        const std::unordered_map<std::string, std::string>& contextData = {}) override {
        
        return createFullCheckpoint(workflowName, executionId, type, contextData, {});
    }
    
    std::string createFullCheckpoint(
        const std::string& workflowName,
        const std::string& executionId,
        CheckpointType type,
        const std::unordered_map<std::string, std::string>& contextData,
        const std::unordered_map<std::string, Any>& contextObjects) override {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string checkpointId = generateCheckpointId(workflowName, executionId);
        
        Checkpoint checkpoint;
        checkpoint.checkpointId = checkpointId;
        checkpoint.workflowName = workflowName;
        checkpoint.executionId = executionId;
        checkpoint.type = type;
        checkpoint.status = CheckpointStatus::VALID;
        checkpoint.createdAt = TimeUtils::getCurrentMs();
        checkpoint.expiresAt = checkpoint.createdAt + config_.checkpointExpirationMs;
        checkpoint.contextData = contextData;
        checkpoint.contextObjects = contextObjects;
        checkpoint.version = 1;
        
        checkpoints_[checkpointId] = checkpoint;
        
        workflowCheckpoints_[workflowName].push_back(checkpointId);
        executionCheckpoints_[executionId].push_back(checkpointId);
        
        enforceMaxCheckpoints(workflowName);
        
        logger_.info("创建检查点: " + checkpointId + " 工作流: " + workflowName);
        
        return checkpointId;
    }
    
    bool updateCheckpoint(
        const std::string& checkpointId,
        const std::unordered_map<std::string, std::string>& contextData) override {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = checkpoints_.find(checkpointId);
        if (it == checkpoints_.end()) {
            logger_.warning("检查点不存在: " + checkpointId);
            return false;
        }
        
        if (it->second.status != CheckpointStatus::VALID) {
            logger_.warning("检查点无效: " + checkpointId);
            return false;
        }
        
        for (const auto& pair : contextData) {
            it->second.contextData[pair.first] = pair.second;
        }
        it->second.version++;
        
        logger_.info("更新检查点: " + checkpointId);
        return true;
    }
    
    // ========== 检查点恢复 ==========
    
    bool restoreCheckpoint(
        const std::string& checkpointId,
        const RestoreOptions& options = RestoreOptions()) override {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = checkpoints_.find(checkpointId);
        if (it == checkpoints_.end()) {
            logger_.error("检查点不存在: " + checkpointId);
            return false;
        }
        
        if (options.validateCheckpoint && !validateCheckpointInternal(it->second)) {
            logger_.error("检查点验证失败: " + checkpointId);
            return false;
        }
        
        it->second.status = CheckpointStatus::RESTORED;
        
        logger_.info("恢复检查点: " + checkpointId);
        return true;
    }
    
    std::string restoreLatestCheckpoint(
        const std::string& workflowName,
        const RestoreOptions& options = RestoreOptions()) override {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        Checkpoint latest = getLatestCheckpointInternal(workflowName);
        if (latest.checkpointId.empty()) {
            logger_.warning("工作流没有可用检查点: " + workflowName);
            return "";
        }
        
        if (options.validateCheckpoint && !validateCheckpointInternal(latest)) {
            logger_.error("最新检查点验证失败: " + latest.checkpointId);
            return "";
        }
        
        latest.status = CheckpointStatus::RESTORED;
        checkpoints_[latest.checkpointId] = latest;
        
        logger_.info("恢复最新检查点: " + latest.checkpointId);
        return latest.checkpointId;
    }
    
    bool validateCheckpoint(const std::string& checkpointId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = checkpoints_.find(checkpointId);
        if (it == checkpoints_.end()) {
            return false;
        }
        
        return validateCheckpointInternal(it->second);
    }
    
    // ========== 查询接口 ==========
    
    Checkpoint getCheckpoint(const std::string& checkpointId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = checkpoints_.find(checkpointId);
        if (it == checkpoints_.end()) {
            return Checkpoint();
        }
        
        return it->second;
    }
    
    std::vector<Checkpoint> getCheckpointsByWorkflow(const std::string& workflowName) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Checkpoint> result;
        auto it = workflowCheckpoints_.find(workflowName);
        if (it == workflowCheckpoints_.end()) {
            return result;
        }
        
        for (const auto& checkpointId : it->second) {
            auto cpIt = checkpoints_.find(checkpointId);
            if (cpIt != checkpoints_.end()) {
                result.push_back(cpIt->second);
            }
        }
        
        std::sort(result.begin(), result.end(), 
            [](const Checkpoint& a, const Checkpoint& b) {
                return a.createdAt > b.createdAt;
            });
        
        return result;
    }
    
    std::vector<Checkpoint> getCheckpointsByExecution(const std::string& executionId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Checkpoint> result;
        auto it = executionCheckpoints_.find(executionId);
        if (it == executionCheckpoints_.end()) {
            return result;
        }
        
        for (const auto& checkpointId : it->second) {
            auto cpIt = checkpoints_.find(checkpointId);
            if (cpIt != checkpoints_.end()) {
                result.push_back(cpIt->second);
            }
        }
        
        std::sort(result.begin(), result.end(), 
            [](const Checkpoint& a, const Checkpoint& b) {
                return a.createdAt > b.createdAt;
            });
        
        return result;
    }
    
    Checkpoint getLatestCheckpoint(const std::string& workflowName) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return getLatestCheckpointInternal(workflowName);
    }
    
    bool hasRestorableCheckpoint(const std::string& workflowName) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Checkpoint latest = getLatestCheckpointInternal(workflowName);
        if (latest.checkpointId.empty()) {
            return false;
        }
        
        return validateCheckpointInternal(latest);
    }
    
    // ========== 管理操作 ==========
    
    bool deleteCheckpoint(const std::string& checkpointId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = checkpoints_.find(checkpointId);
        if (it == checkpoints_.end()) {
            return false;
        }
        
        std::string workflowName = it->second.workflowName;
        std::string executionId = it->second.executionId;
        
        checkpoints_.erase(it);
        
        auto& wfList = workflowCheckpoints_[workflowName];
        wfList.erase(std::remove(wfList.begin(), wfList.end(), checkpointId), wfList.end());
        
        auto& execList = executionCheckpoints_[executionId];
        execList.erase(std::remove(execList.begin(), execList.end(), checkpointId), execList.end());
        
        logger_.info("删除检查点: " + checkpointId);
        return true;
    }
    
    int deleteCheckpointsByWorkflow(const std::string& workflowName) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = workflowCheckpoints_.find(workflowName);
        if (it == workflowCheckpoints_.end()) {
            return 0;
        }
        
        int count = 0;
        for (const auto& checkpointId : it->second) {
            auto cpIt = checkpoints_.find(checkpointId);
            if (cpIt != checkpoints_.end()) {
                std::string executionId = cpIt->second.executionId;
                auto& execList = executionCheckpoints_[executionId];
                execList.erase(std::remove(execList.begin(), execList.end(), checkpointId), execList.end());
                checkpoints_.erase(cpIt);
                count++;
            }
        }
        
        workflowCheckpoints_.erase(it);
        
        logger_.info("删除工作流检查点: " + workflowName + " 数量: " + std::to_string(count));
        return count;
    }
    
    int cleanupExpiredCheckpoints() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int64_t now = TimeUtils::getCurrentMs();
        int count = 0;
        
        std::vector<std::string> toDelete;
        for (auto& pair : checkpoints_) {
            if (pair.second.expiresAt > 0 && pair.second.expiresAt < now) {
                pair.second.status = CheckpointStatus::EXPIRED;
                toDelete.push_back(pair.first);
                count++;
            }
        }
        
        for (const auto& checkpointId : toDelete) {
            auto it = checkpoints_.find(checkpointId);
            if (it != checkpoints_.end()) {
                std::string workflowName = it->second.workflowName;
                std::string executionId = it->second.executionId;
                
                auto& wfList = workflowCheckpoints_[workflowName];
                wfList.erase(std::remove(wfList.begin(), wfList.end(), checkpointId), wfList.end());
                
                auto& execList = executionCheckpoints_[executionId];
                execList.erase(std::remove(execList.begin(), execList.end(), checkpointId), execList.end());
                
                checkpoints_.erase(it);
            }
        }
        
        if (count > 0) {
            logger_.info("清理过期检查点数量: " + std::to_string(count));
        }
        
        return count;
    }
    
    void setConfig(const CheckpointConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }
    
    CheckpointConfig getConfig() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    // ========== 导入导出 ==========
    
    std::string exportCheckpoint(const std::string& checkpointId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = checkpoints_.find(checkpointId);
        if (it == checkpoints_.end()) {
            return "";
        }
        
        const Checkpoint& cp = it->second;
        
        std::ostringstream oss;
        oss << "{";
        oss << "\"checkpointId\":\"" << cp.checkpointId << "\",";
        oss << "\"workflowName\":\"" << cp.workflowName << "\",";
        oss << "\"executionId\":\"" << cp.executionId << "\",";
        oss << "\"type\":\"" << checkpointTypeToString(cp.type) << "\",";
        oss << "\"status\":\"" << checkpointStatusToString(cp.status) << "\",";
        oss << "\"createdAt\":" << cp.createdAt << ",";
        oss << "\"expiresAt\":" << cp.expiresAt << ",";
        oss << "\"workflowState\":\"" << workflowStateToString(cp.workflowState) << "\",";
        oss << "\"currentNodeId\":\"" << cp.currentNodeId << "\",";
        oss << "\"progress\":" << cp.progress << ",";
        oss << "\"version\":" << cp.version << ",";
        oss << "\"contextData\":{";
        bool first = true;
        for (const auto& pair : cp.contextData) {
            if (!first) oss << ",";
            oss << "\"" << pair.first << "\":\"" << escapeJson(pair.second) << "\"";
            first = false;
        }
        oss << "},";
        oss << "\"metadata\":\"" << escapeJson(cp.metadata) << "\"";
        oss << "}";
        
        return oss.str();
    }
    
    std::string importCheckpoint(const std::string& jsonData) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Checkpoint cp = parseCheckpointJson(jsonData);
        if (cp.workflowName.empty()) {
            logger_.error("导入检查点失败: 无效的JSON数据");
            return "";
        }
        
        std::string newId = generateCheckpointId(cp.workflowName, cp.executionId);
        cp.checkpointId = newId;
        cp.createdAt = TimeUtils::getCurrentMs();
        cp.expiresAt = cp.createdAt + config_.checkpointExpirationMs;
        
        checkpoints_[newId] = cp;
        workflowCheckpoints_[cp.workflowName].push_back(newId);
        executionCheckpoints_[cp.executionId].push_back(newId);
        
        logger_.info("导入检查点: " + newId);
        return newId;
    }
    
    // ========== 统计 ==========
    
    size_t getCheckpointCount() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return checkpoints_.size();
    }
    
    int64_t getStorageSize() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int64_t size = 0;
        for (const auto& pair : checkpoints_) {
            const Checkpoint& cp = pair.second;
            size += cp.checkpointId.size() + cp.workflowName.size() + cp.executionId.size();
            size += cp.currentNodeId.size() + cp.metadata.size();
            for (const auto& data : cp.contextData) {
                size += data.first.size() + data.second.size();
            }
            size += 200;
        }
        
        return size;
    }

private:
    mutable std::mutex mutex_;
    CheckpointConfig config_;
    Logger& logger_;
    
    std::unordered_map<std::string, Checkpoint> checkpoints_;
    std::unordered_map<std::string, std::vector<std::string>> workflowCheckpoints_;
    std::unordered_map<std::string, std::vector<std::string>> executionCheckpoints_;
    
    std::string generateCheckpointId(const std::string& workflowName, const std::string& executionId) {
        int64_t now = TimeUtils::getCurrentMs();
        return "cp_" + workflowName + "_" + executionId + "_" + std::to_string(now);
    }
    
    Checkpoint getLatestCheckpointInternal(const std::string& workflowName) const {
        auto it = workflowCheckpoints_.find(workflowName);
        if (it == workflowCheckpoints_.end() || it->second.empty()) {
            return Checkpoint();
        }
        
        Checkpoint latest;
        int64_t latestTime = 0;
        
        for (const auto& checkpointId : it->second) {
            auto cpIt = checkpoints_.find(checkpointId);
            if (cpIt != checkpoints_.end() && 
                cpIt->second.status == CheckpointStatus::VALID &&
                cpIt->second.createdAt > latestTime) {
                latest = cpIt->second;
                latestTime = cpIt->second.createdAt;
            }
        }
        
        return latest;
    }
    
    bool validateCheckpointInternal(const Checkpoint& checkpoint) const {
        if (checkpoint.status != CheckpointStatus::VALID) {
            return false;
        }
        
        int64_t now = TimeUtils::getCurrentMs();
        if (checkpoint.expiresAt > 0 && checkpoint.expiresAt < now) {
            return false;
        }
        
        if (checkpoint.workflowName.empty() || checkpoint.executionId.empty()) {
            return false;
        }
        
        return true;
    }
    
    void enforceMaxCheckpoints(const std::string& workflowName) {
        auto it = workflowCheckpoints_.find(workflowName);
        if (it == workflowCheckpoints_.end()) {
            return;
        }
        
        while (it->second.size() > static_cast<size_t>(config_.maxCheckpoints)) {
            std::string oldestId = it->second.front();
            it->second.erase(it->second.begin());
            
            auto cpIt = checkpoints_.find(oldestId);
            if (cpIt != checkpoints_.end()) {
                std::string executionId = cpIt->second.executionId;
                auto& execList = executionCheckpoints_[executionId];
                execList.erase(std::remove(execList.begin(), execList.end(), oldestId), execList.end());
                checkpoints_.erase(cpIt);
            }
            
            logger_.info("删除最旧检查点: " + oldestId);
        }
    }
    
    std::string escapeJson(const std::string& str) const {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
    
    Checkpoint parseCheckpointJson(const std::string& json) const {
        Checkpoint cp;
        
        auto extractString = [&json](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":\"";
            size_t pos = json.find(searchKey);
            if (pos == std::string::npos) {
                searchKey = "\"" + key + "\": \"";
                pos = json.find(searchKey);
                if (pos == std::string::npos) return "";
            }
            pos += searchKey.length();
            size_t endPos = json.find("\"", pos);
            if (endPos == std::string::npos) return "";
            return json.substr(pos, endPos - pos);
        };
        
        auto extractInt = [&json](const std::string& key) -> int {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = json.find(searchKey);
            if (pos == std::string::npos) return 0;
            pos += searchKey.length();
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
            size_t endPos = pos;
            while (endPos < json.size() && (json[endPos] >= '0' && json[endPos] <= '9')) endPos++;
            if (endPos == pos) return 0;
            return std::stoi(json.substr(pos, endPos - pos));
        };
        
        auto extractInt64 = [&json](const std::string& key) -> int64_t {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = json.find(searchKey);
            if (pos == std::string::npos) return 0;
            pos += searchKey.length();
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
            size_t endPos = pos;
            while (endPos < json.size() && (json[endPos] >= '0' && json[endPos] <= '9')) endPos++;
            if (endPos == pos) return 0;
            return std::stoll(json.substr(pos, endPos - pos));
        };
        
        cp.workflowName = extractString("workflowName");
        cp.executionId = extractString("executionId");
        cp.currentNodeId = extractString("currentNodeId");
        cp.metadata = extractString("metadata");
        cp.createdAt = extractInt64("createdAt");
        cp.expiresAt = extractInt64("expiresAt");
        cp.progress = extractInt("progress");
        cp.version = extractInt("version");
        
        std::string typeStr = extractString("type");
        if (typeStr == "automatic") cp.type = CheckpointType::AUTOMATIC;
        else if (typeStr == "manual") cp.type = CheckpointType::MANUAL;
        else if (typeStr == "before_retry") cp.type = CheckpointType::BEFORE_RETRY;
        else if (typeStr == "after_node") cp.type = CheckpointType::AFTER_NODE;
        else if (typeStr == "on_pause") cp.type = CheckpointType::ON_PAUSE;
        else if (typeStr == "on_error") cp.type = CheckpointType::ON_ERROR;
        
        std::string statusStr = extractString("status");
        if (statusStr == "valid") cp.status = CheckpointStatus::VALID;
        else if (statusStr == "expired") cp.status = CheckpointStatus::EXPIRED;
        else if (statusStr == "corrupted") cp.status = CheckpointStatus::CORRUPTED;
        else if (statusStr == "restored") cp.status = CheckpointStatus::RESTORED;
        
        std::string stateStr = extractString("workflowState");
        if (stateStr == "idle") cp.workflowState = WorkflowState::IDLE;
        else if (stateStr == "running") cp.workflowState = WorkflowState::RUNNING;
        else if (stateStr == "paused") cp.workflowState = WorkflowState::PAUSED;
        else if (stateStr == "completed") cp.workflowState = WorkflowState::COMPLETED;
        else if (stateStr == "failed") cp.workflowState = WorkflowState::FAILED;
        else if (stateStr == "cancelled") cp.workflowState = WorkflowState::CANCELED;
        
        return cp;
    }
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CHECKPOINT_MANAGER_IMPL_H
