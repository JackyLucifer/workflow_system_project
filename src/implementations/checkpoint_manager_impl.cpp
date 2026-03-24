/**
 * @file checkpoint_manager_impl.cpp
 * @brief 检查点管理器实现
 */

#include "workflow_system/implementations/checkpoint_manager_impl.h"
#include "workflow_system/core/logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace WorkflowSystem {

CheckpointManagerImpl::CheckpointManagerImpl(const std::string& storagePath)
    : storagePath_(storagePath)
    , autoSaveEnabled_(false)
    , currentWorkflow_(nullptr)
    , currentContext_(nullptr)
    , checkpointCounter_(0) {

    // 确保存储目录存在
    createDirectoryIfNotExists(storagePath_);

    // 加载已有检查点
    loadCheckpoints();

    LOG_INFO("检查点管理器初始化完成，存储路径: " + storagePath);
}

CheckpointManagerImpl::~CheckpointManagerImpl() {
    disableAutoSave();
}

CheckpointInfo CheckpointManagerImpl::createCheckpoint(
    const std::string& workflowId,
    const std::string& nodeId,
    const WorkflowState& state) {

    std::lock_guard<std::mutex> lock(mutex_);

    CheckpointInfo info;
    info.checkpointId = generateId();
    info.workflowId = workflowId;
    info.nodeId = nodeId;
    info.state = state;
    info.timestamp = std::chrono::system_clock::now();

    // 序列化状态数据
    CheckpointData data;
    data.workflowId = workflowId;
    data.nodeId = nodeId;
    data.stateValue = static_cast<int>(state);
    data.timestamp = std::chrono::system_clock::to_time_t(info.timestamp);

    // 保存工作流上下文数据
    if (currentContext_) {
        data.contextData = currentContext_->getAllData();
    }

    // 序列化并保存到文件
    std::string serialized = serializeCheckpoint(data);
    std::string filePath = getCheckpointPath(info.checkpointId);

    if (writeToFile(filePath, serialized)) {
        checkpoints_[info.checkpointId] = info;
        LOG_INFO("创建检查点: " + info.checkpointId + " for workflow: " + workflowId);
    } else {
        LOG_ERROR("创建检查点失败: " + info.checkpointId);
    }

    return info;
}

bool CheckpointManagerImpl::restoreFromCheckpoint(
    const std::string& checkpointId,
    IWorkflowContext* context) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = checkpoints_.find(checkpointId);
    if (it == checkpoints_.end()) {
        LOG_WARNING("检查点不存在: " + checkpointId);
        return false;
    }

    std::string filePath = getCheckpointPath(checkpointId);
    std::string serialized = readFile(filePath);

    if (serialized.empty()) {
        LOG_ERROR("读取检查点文件失败: " + filePath);
        return false;
    }

    CheckpointData data = deserializeCheckpoint(serialized);

    // 恢复上下文数据
    if (context) {
        for (const auto& pair : data.contextData) {
            context->set(pair.first, pair.second);
        }
    }

    LOG_INFO("从检查点恢复: " + checkpointId);
    return true;
}

std::vector<CheckpointInfo> CheckpointManagerImpl::listCheckpoints(
    const std::string& workflowId) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<CheckpointInfo> result;
    for (const auto& pair : checkpoints_) {
        if (workflowId.empty() || pair.second.workflowId == workflowId) {
            result.push_back(pair.second);
        }
    }

    // 按时间排序（最新的在前）
    std::sort(result.begin(), result.end(),
        [](const CheckpointInfo& a, const CheckpointInfo& b) {
            return a.timestamp > b.timestamp;
        });

    return result;
}

bool CheckpointManagerImpl::deleteCheckpoint(const std::string& checkpointId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = checkpoints_.find(checkpointId);
    if (it == checkpoints_.end()) {
        return false;
    }

    std::string filePath = getCheckpointPath(checkpointId);
    if (std::remove(filePath.c_str()) == 0) {
        checkpoints_.erase(it);
        LOG_INFO("删除检查点: " + checkpointId);
        return true;
    }

    return false;
}

void CheckpointManagerImpl::enableAutoSave(int intervalSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (autoSaveEnabled_) {
        LOG_WARNING("自动保存已启用");
        return;
    }

    autoSaveEnabled_ = true;
    autoSaveInterval_ = std::chrono::seconds(intervalSeconds);

    autoSaveThread_ = std::thread([this]() {
        while (autoSaveEnabled_) {
            std::this_thread::sleep_for(autoSaveInterval_);
            if (autoSaveEnabled_ && currentWorkflow_ && currentContext_) {
                createAutoCheckpoint();
            }
        }
    });

    LOG_INFO("自动保存已启用，间隔: " + std::to_string(intervalSeconds) + "秒");
}

void CheckpointManagerImpl::disableAutoSave() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!autoSaveEnabled_) {
        return;
    }

    autoSaveEnabled_ = false;

    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }

    LOG_INFO("自动保存已禁用");
}

void CheckpointManagerImpl::setCurrentWorkflow(
    IWorkflow* workflow,
    IWorkflowContext* context,
    const std::string& currentNodeId) {

    std::lock_guard<std::mutex> lock(mutex_);
    currentWorkflow_ = workflow;
    currentContext_ = context;
    currentNodeId_ = currentNodeId;
}

void CheckpointManagerImpl::clearOldCheckpoints(
    const std::string& workflowId,
    size_t keepCount) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<CheckpointInfo> workflowCheckpoints;
    for (const auto& pair : checkpoints_) {
        if (pair.second.workflowId == workflowId) {
            workflowCheckpoints.push_back(pair.second);
        }
    }

    // 按时间排序
    std::sort(workflowCheckpoints.begin(), workflowCheckpoints.end(),
        [](const CheckpointInfo& a, const CheckpointInfo& b) {
            return a.timestamp < b.timestamp;  // 最旧的在前
        });

    // 删除旧的检查点
    size_t toDelete = workflowCheckpoints.size() > keepCount
        ? workflowCheckpoints.size() - keepCount
        : 0;

    for (size_t i = 0; i < toDelete; ++i) {
        deleteCheckpoint(workflowCheckpoints[i].checkpointId);
    }

    if (toDelete > 0) {
        LOG_INFO("清理了 " + std::to_string(toDelete) + " 个旧检查点");
    }
}

// 私有方法实现

std::string CheckpointManagerImpl::generateId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto counter = checkpointCounter_.fetch_add(1);

    std::ostringstream oss;
    oss << "cp_" << time_t << "_" << counter;
    return oss.str();
}

std::string CheckpointManagerImpl::getCheckpointPath(const std::string& id) const {
    return storagePath_ + "/" + id + ".checkpoint";
}

std::string CheckpointManagerImpl::serializeCheckpoint(const CheckpointData& data) {
    std::ostringstream oss;

    oss << "CheckpointData\n";
    oss << "version=1.0\n";
    oss << "workflowId=" << data.workflowId << "\n";
    oss << "nodeId=" << data.nodeId << "\n";
    oss << "state=" << data.stateValue << "\n";
    oss << "timestamp=" << data.timestamp << "\n";
    oss << "dataCount=" << data.contextData.size() << "\n";

    for (const auto& pair : data.contextData) {
        oss << pair.first << "=" << pair.second << "\n";
    }

    return oss.str();
}

CheckpointData CheckpointManagerImpl::deserializeCheckpoint(const std::string& serialized) {
    CheckpointData data;
    std::istringstream iss(serialized);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (key == "workflowId") {
            data.workflowId = value;
        } else if (key == "nodeId") {
            data.nodeId = value;
        } else if (key == "state") {
            data.stateValue = std::stoi(value);
        } else if (key == "timestamp") {
            data.timestamp = std::stoll(value);
        } else if (key == "dataCount") {
            // 读取数据数量
        } else {
            // 上下文数据
            data.contextData[key] = value;
        }
    }

    return data;
}

bool CheckpointManagerImpl::writeToFile(const std::string& path,
                                        const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    return file.good();
}

std::string CheckpointManagerImpl::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

void CheckpointManagerImpl::createDirectoryIfNotExists(const std::string& path) {
    // 简单实现：使用系统命令
    std::string cmd = "mkdir -p " + path;
    system(cmd.c_str());
}

void CheckpointManagerImpl::loadCheckpoints() {
    // 扫描目录，加载已有检查点
    std::string cmd = "ls " + storagePath_ + "/*.checkpoint 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string filePath(buffer);
        filePath.erase(filePath.find_last_not_of("\n\r") + 1);

        // 从文件名提取检查点ID
        std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
        std::string checkpointId = filename.substr(0, filename.find(".checkpoint"));

        // 读取检查点信息
        std::string content = readFile(filePath);
        if (!content.empty()) {
            CheckpointData data = deserializeCheckpoint(content);

            CheckpointInfo info;
            info.checkpointId = checkpointId;
            info.workflowId = data.workflowId;
            info.nodeId = data.nodeId;
            info.state = static_cast<WorkflowState>(data.stateValue);
            info.timestamp = std::chrono::system_clock::from_time_t(data.timestamp);

            checkpoints_[checkpointId] = info;
        }
    }

    pclose(pipe);
}

void CheckpointManagerImpl::createAutoCheckpoint() {
    if (!currentWorkflow_ || !currentContext_) {
        return;
    }

    try {
        createCheckpoint(
            currentWorkflow_->getName(),
            currentNodeId_,
            currentWorkflow_->getState()
        );
    } catch (const std::exception& e) {
        LOG_ERROR("自动创建检查点失败: " + std::string(e.what()));
    }
}

} // namespace WorkflowSystem
