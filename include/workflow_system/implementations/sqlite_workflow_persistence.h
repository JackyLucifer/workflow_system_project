/**
 * @file sqlite_workflow_persistence.h
 * @brief 业务流程管理系统 - SQLite 工作流持久化实现
 *
 * 特性：
 * - SQLite3 嵌入式数据库
 * - 事务支持
 * - 参数化查询（防止SQL注入）
 * - 线程安全（互斥锁保护）
 *
 * 依赖：
 * - SQLite3 C API（如果系统提供）
 * - 如果没有系统 SQLite，需要编译时链接 sqlite3 库
 */

#ifndef WORKFLOW_SYSTEM_SQLITE_WORKFLOW_PERSISTENCE_H
#define WORKFLOW_SYSTEM_SQLITE_WORKFLOW_PERSISTENCE_H

#include "workflow_system/interfaces/workflow_persistence.h"
#include "workflow_system/core/logger.h"
#include <mutex>
#include <memory>

// SQLite3 API 头文件
extern "C" {
    struct sqlite3;
    struct sqlite3_stmt;
}

namespace WorkflowSystem {

/**
 * @brief SQLite 工作流持久化实现
 *
 * 数据库表结构：
 * - workflows: 工作流执行记录表
 * - workflow_definitions: 工作流定义存储表
 *
 * SQL 建表语句：
 * CREATE TABLE IF NOT EXISTS workflows (
 *     id TEXT PRIMARY KEY,
 *     workflow_name TEXT NOT NULL,
 *     state INTEGER NOT NULL,
 *     start_time INTEGER NOT NULL,
 *     end_time INTEGER NOT NULL,
 *     duration INTEGER NOT NULL,
 *     success INTEGER NOT NULL,
 *     error_message TEXT,
 *     retry_count INTEGER DEFAULT 0,
 *     input_config TEXT
 * );
 *
 * CREATE TABLE IF NOT EXISTS workflow_definitions (
 *     workflow_id TEXT PRIMARY KEY,
 *     definition_json TEXT NOT NULL,
 *     created_at INTEGER NOT NULL
 * );
 */
class SqliteWorkflowPersistence : public IWorkflowPersistence {
public:
    /**
     * @brief 获取单例实例
     */
    static SqliteWorkflowPersistence& getInstance() {
        static SqliteWorkflowPersistence instance;
        return instance;
    }

    // 禁止拷贝构造和赋值
    SqliteWorkflowPersistence(const SqliteWorkflowPersistence&) = delete;
    SqliteWorkflowPersistence& operator=(const SqliteWorkflowPersistence&) = delete;

    virtual ~SqliteWorkflowPersistence() override;

    // IWorkflowPersistence 接口实现
    bool initialize(const std::string& databasePath) override;
    void close() override;

    bool saveWorkflow(const WorkflowRecord& record) override;
    WorkflowRecord getWorkflowById(const std::string& workflowId) const override;
    std::vector<WorkflowRecord> getAllWorkflows() const override;
    std::vector<WorkflowRecord> getWorkflowsByName(const std::string& name) const override;
    std::vector<WorkflowRecord> getWorkflowsByState(WorkflowState state) const override;
    std::vector<WorkflowRecord> getWorkflowsByTimeRange(int64_t startTime, int64_t endTime) const override;

    bool deleteWorkflow(const std::string& workflowId) override;
    bool clearAllWorkflows() override;

    int getTotalWorkflowCount() const override;
    int getSuccessCount() const override;
    int getFailureCount() const override;
    double getSuccessRate() const override;
    int64_t getAverageDuration() const override;

    bool saveWorkflowDefinition(const std::string& workflowId,
                                     const std::string& workflowJson) override;
    std::string getWorkflowDefinition(const std::string& workflowId) const override;
    std::map<std::string, std::string> getAllWorkflowDefinitions() const override;

private:
    SqliteWorkflowPersistence();
    bool createTables();
    bool prepareStatements();
    void finalizeStatements();
    std::string generateWorkflowId();

    // SQLite3 数据库连接
    sqlite3* db_ = nullptr;
    std::string databasePath_;

    // 互斥锁，保护数据库访问
    mutable std::mutex mutex_;

    // 预编译的 SQL 语句
    sqlite3_stmt* stmtSaveWorkflow_ = nullptr;
    sqlite3_stmt* stmtGetWorkflowById_ = nullptr;
    sqlite3_stmt* stmtGetAllWorkflows_ = nullptr;
    sqlite3_stmt* stmtGetWorkflowsByName_ = nullptr;
    sqlite3_stmt* stmtGetWorkflowsByState_ = nullptr;
    sqlite3_stmt* stmtGetWorkflowsByTimeRange_ = nullptr;
    sqlite3_stmt* stmtDeleteWorkflow_ = nullptr;
    sqlite3_stmt* stmtClearAllWorkflows_ = nullptr;

    sqlite3_stmt* stmtGetTotalCount_ = nullptr;
    sqlite3_stmt* stmtGetSuccessCount_ = nullptr;
    sqlite3_stmt* stmtGetFailureCount_ = nullptr;
    sqlite3_stmt* stmtGetSuccessRate_ = nullptr;
    sqlite3_stmt* stmtGetAverageDuration_ = nullptr;

    sqlite3_stmt* stmtSaveDefinition_ = nullptr;
    sqlite3_stmt* stmtGetDefinition_ = nullptr;
    sqlite3_stmt* stmtGetAllDefinitions_ = nullptr;

    bool initialized_ = false;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_SQLITE_WORKFLOW_PERSISTENCE_H
