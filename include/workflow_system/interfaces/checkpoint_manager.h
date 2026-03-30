/**
 * @file checkpoint_manager.h
 * @brief 检查点管理器接口 - 断点续传支持
 *
 * 功能：工作流状态快照、断点恢复、进度保存
 */

#ifndef WORKFLOW_SYSTEM_CHECKPOINT_MANAGER_H
#define WORKFLOW_SYSTEM_CHECKPOINT_MANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "workflow_system/core/types.h"
#include "workflow_system/core/any.h"

namespace WorkflowSystem {

/**
 * @brief 检查点类型
 */
enum class CheckpointType {
    AUTOMATIC,      // 自动检查点
    MANUAL,         // 手动检查点
    BEFORE_RETRY,   // 重试前
    AFTER_NODE,     // 节点完成后
    ON_PAUSE,       // 暂停时
    ON_ERROR        // 错误时
};

/**
 * @brief 检查点状态
 */
enum class CheckpointStatus {
    VALID,          // 有效
    EXPIRED,        // 已过期
    CORRUPTED,      // 已损坏
    RESTORED        // 已恢复
};

/**
 * @brief 检查点数据
 */
struct Checkpoint {
    std::string checkpointId;               // 检查点ID
    std::string workflowName;               // 工作流名称
    std::string executionId;                // 执行ID
    CheckpointType type;                    // 类型
    CheckpointStatus status;                // 状态
    int64_t createdAt;                      // 创建时间
    int64_t expiresAt;                      // 过期时间
    WorkflowState workflowState;            // 工作流状态
    std::string currentNodeId;              // 当前节点ID
    int progress;                           // 进度百分比 (0-100)
    std::unordered_map<std::string, std::string> contextData;  // 上下文数据
    std::unordered_map<std::string, Any> contextObjects;       // 上下文对象
    std::string metadata;                   // 元数据 (JSON格式)
    int version;                            // 版本号
    
    Checkpoint()
        : type(CheckpointType::AUTOMATIC)
        , status(CheckpointStatus::VALID)
        , createdAt(0)
        , expiresAt(0)
        , workflowState(WorkflowState::IDLE)
        , progress(0)
        , version(1) {}
};

/**
 * @brief 检查点配置
 */
struct CheckpointConfig {
    bool enabled;                            // 是否启用
    int64_t autoCheckpointIntervalMs;        // 自动检查点间隔
    int maxCheckpoints;                      // 最大检查点数量
    int64_t checkpointExpirationMs;          // 检查点过期时间
    bool checkpointOnPause;                  // 暂停时创建检查点
    bool checkpointOnError;                  // 错误时创建检查点
    bool checkpointAfterNode;                // 节点完成后创建检查点
    std::string storagePath;                 // 存储路径
    
    CheckpointConfig()
        : enabled(true)
        , autoCheckpointIntervalMs(60000)
        , maxCheckpoints(100)
        , checkpointExpirationMs(24 * 60 * 60 * 1000)
        , checkpointOnPause(true)
        , checkpointOnError(true)
        , checkpointAfterNode(false) {}
};

/**
 * @brief 恢复选项
 */
struct RestoreOptions {
    bool restoreContext;                     // 恢复上下文
    bool restoreState;                       // 恢复状态
    bool restoreProgress;                    // 恢复进度
    bool validateCheckpoint;                 // 验证检查点
    bool notifyObservers;                    // 通知观察者
    std::string startingNodeId;              // 起始节点ID (可选)
    
    RestoreOptions()
        : restoreContext(true)
        , restoreState(true)
        , restoreProgress(true)
        , validateCheckpoint(true)
        , notifyObservers(true) {}
};

/**
 * @brief 检查点管理器接口
 */
class ICheckpointManager {
public:
    virtual ~ICheckpointManager() = default;
    
    // ========== 检查点创建 ==========
    
    /**
     * @brief 创建检查点
     * @param workflowName 工作流名称
     * @param executionId 执行ID
     * @param type 检查点类型
     * @param contextData 上下文数据
     * @return 检查点ID
     */
    virtual std::string createCheckpoint(
        const std::string& workflowName,
        const std::string& executionId,
        CheckpointType type,
        const std::unordered_map<std::string, std::string>& contextData = {}) = 0;
    
    /**
     * @brief 创建完整检查点（包含对象）
     */
    virtual std::string createFullCheckpoint(
        const std::string& workflowName,
        const std::string& executionId,
        CheckpointType type,
        const std::unordered_map<std::string, std::string>& contextData,
        const std::unordered_map<std::string, Any>& contextObjects) = 0;
    
    /**
     * @brief 更新检查点
     * @param checkpointId 检查点ID
     * @param contextData 新的上下文数据
     * @return 是否成功
     */
    virtual bool updateCheckpoint(
        const std::string& checkpointId,
        const std::unordered_map<std::string, std::string>& contextData) = 0;
    
    // ========== 检查点恢复 ==========
    
    /**
     * @brief 恢复到指定检查点
     * @param checkpointId 检查点ID
     * @param options 恢复选项
     * @return 是否成功
     */
    virtual bool restoreCheckpoint(
        const std::string& checkpointId,
        const RestoreOptions& options = RestoreOptions()) = 0;
    
    /**
     * @brief 恢复到最新检查点
     * @param workflowName 工作流名称
     * @param options 恢复选项
     * @return 检查点ID，失败返回空字符串
     */
    virtual std::string restoreLatestCheckpoint(
        const std::string& workflowName,
        const RestoreOptions& options = RestoreOptions()) = 0;
    
    /**
     * @brief 验证检查点
     * @param checkpointId 检查点ID
     * @return 是否有效
     */
    virtual bool validateCheckpoint(const std::string& checkpointId) = 0;
    
    // ========== 查询接口 ==========
    
    /**
     * @brief 获取检查点
     * @param checkpointId 检查点ID
     * @return 检查点数据
     */
    virtual Checkpoint getCheckpoint(const std::string& checkpointId) const = 0;
    
    /**
     * @brief 获取工作流的所有检查点
     * @param workflowName 工作流名称
     * @return 检查点列表
     */
    virtual std::vector<Checkpoint> getCheckpointsByWorkflow(
        const std::string& workflowName) const = 0;
    
    /**
     * @brief 获取执行的检查点列表
     * @param executionId 执行ID
     * @return 检查点列表
     */
    virtual std::vector<Checkpoint> getCheckpointsByExecution(
        const std::string& executionId) const = 0;
    
    /**
     * @brief 获取最新检查点
     * @param workflowName 工作流名称
     * @return 检查点数据
     */
    virtual Checkpoint getLatestCheckpoint(const std::string& workflowName) const = 0;
    
    /**
     * @brief 检查是否有可恢复的检查点
     * @param workflowName 工作流名称
     * @return 是否存在
     */
    virtual bool hasRestorableCheckpoint(const std::string& workflowName) const = 0;
    
    // ========== 管理操作 ==========
    
    /**
     * @brief 删除检查点
     * @param checkpointId 检查点ID
     * @return 是否成功
     */
    virtual bool deleteCheckpoint(const std::string& checkpointId) = 0;
    
    /**
     * @brief 删除工作流的所有检查点
     * @param workflowName 工作流名称
     * @return 删除数量
     */
    virtual int deleteCheckpointsByWorkflow(const std::string& workflowName) = 0;
    
    /**
     * @brief 清理过期检查点
     * @return 清理数量
     */
    virtual int cleanupExpiredCheckpoints() = 0;
    
    /**
     * @brief 设置配置
     * @param config 配置
     */
    virtual void setConfig(const CheckpointConfig& config) = 0;
    
    /**
     * @brief 获取配置
     * @return 配置
     */
    virtual CheckpointConfig getConfig() const = 0;
    
    // ========== 导入导出 ==========
    
    /**
     * @brief 导出检查点
     * @param checkpointId 检查点ID
     * @return JSON格式数据
     */
    virtual std::string exportCheckpoint(const std::string& checkpointId) const = 0;
    
    /**
     * @brief 导入检查点
     * @param jsonData JSON格式数据
     * @return 检查点ID
     */
    virtual std::string importCheckpoint(const std::string& jsonData) = 0;
    
    // ========== 统计 ==========
    
    /**
     * @brief 获取检查点数量
     * @return 数量
     */
    virtual size_t getCheckpointCount() const = 0;
    
    /**
     * @brief 获取存储大小（字节）
     * @return 大小
     */
    virtual int64_t getStorageSize() const = 0;
};

/**
 * @brief 检查点类型转字符串
 */
inline std::string checkpointTypeToString(CheckpointType type) {
    switch (type) {
        case CheckpointType::AUTOMATIC: return "automatic";
        case CheckpointType::MANUAL: return "manual";
        case CheckpointType::BEFORE_RETRY: return "before_retry";
        case CheckpointType::AFTER_NODE: return "after_node";
        case CheckpointType::ON_PAUSE: return "on_pause";
        case CheckpointType::ON_ERROR: return "on_error";
        default: return "unknown";
    }
}

/**
 * @brief 检查点状态转字符串
 */
inline std::string checkpointStatusToString(CheckpointStatus status) {
    switch (status) {
        case CheckpointStatus::VALID: return "valid";
        case CheckpointStatus::EXPIRED: return "expired";
        case CheckpointStatus::CORRUPTED: return "corrupted";
        case CheckpointStatus::RESTORED: return "restored";
        default: return "unknown";
    }
}

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_CHECKPOINT_MANAGER_H
