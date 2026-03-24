/**
 * @file sqlite_workflow_persistence.cpp
 * @brief 业务流程管理系统 - SQLite 工作流持久化实现
 */

#include "implementations/sqlite_workflow_persistence.h"
#include <sqlite3.h>
#include <sstream>
#include <iostream>
#include <random>

namespace WorkflowSystem {

SqliteWorkflowPersistence::SqliteWorkflowPersistence() {
    db_ = nullptr;
    initialized_ = false;
}

SqliteWorkflowPersistence::~SqliteWorkflowPersistence() {
    close();
}

bool SqliteWorkflowPersistence::initialize(const std::string& databasePath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        LOG_WARNING("[Persistence] Already initialized");
        return true;
    }

    databasePath_ = databasePath;
    int result = sqlite3_open_v2(databasePath.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to open database: " + databasePath +
                 " (error: " + std::to_string(result) + ")");
        return false;
    }

    LOG_INFO("[Persistence] Database opened: " + databasePath);

    // 启用外键约束
    const char* foreignKeySql = "PRAGMA foreign_keys = ON;";
    char* errorMsg = nullptr;
    result = sqlite3_exec(db_, foreignKeySql, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::string errStr = errorMsg ? errorMsg : "unknown error";
        LOG_ERROR("[Persistence] Failed to enable foreign keys: " + errStr);
        if (errorMsg) {
            sqlite3_free(errorMsg);
        }
        return false;
    }

    // 创建数据表
    if (!createTables()) {
        LOG_ERROR("[Persistence] Failed to create tables");
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    // 准备 SQL 语句
    if (!prepareStatements()) {
        finalizeStatements();
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    initialized_ = true;
    LOG_INFO("[Persistence] Initialization completed");
    return true;
}

void SqliteWorkflowPersistence::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_) {
        finalizeStatements();
        sqlite3_close(db_);
        db_ = nullptr;
        initialized_ = false;
        LOG_INFO("[Persistence] Database closed");
    }
}

bool SqliteWorkflowPersistence::createTables() {
    const char* createWorkflowsTable = R"(
        CREATE TABLE IF NOT EXISTS workflows (
            id TEXT PRIMARY KEY,
            workflow_name TEXT NOT NULL,
            state INTEGER NOT NULL,
            start_time INTEGER NOT NULL,
            end_time INTEGER NOT NULL,
            duration INTEGER NOT NULL,
            success INTEGER NOT NULL,
            error_message TEXT,
            retry_count INTEGER DEFAULT 0,
            input_config TEXT
        );
    )";

    const char* createDefinitionsTable = R"(
        CREATE TABLE IF NOT EXISTS workflow_definitions (
            workflow_id TEXT PRIMARY KEY,
            definition_json TEXT NOT NULL,
            created_at INTEGER NOT NULL
        );
    )";

    const char* createIndexOnWorkflowName = R"(
        CREATE INDEX IF NOT EXISTS idx_workflow_name ON workflows(workflow_name);
    )";

    const char* createIndexOnState = R"(
        CREATE INDEX IF NOT EXISTS idx_state ON workflows(state);
    )";

    const char* createIndexOnStartTime = R"(
        CREATE INDEX IF NOT EXISTS idx_start_time ON workflows(start_time);
    )";

    // 执行建表语句
    char* errorMsg = nullptr;
    int result = SQLITE_OK;

    result = sqlite3_exec(db_, createWorkflowsTable, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::string errStr = errorMsg ? errorMsg : "unknown error";
        LOG_ERROR("[Persistence] Failed to create workflows table: " + errStr);
        if (errorMsg) sqlite3_free(errorMsg);
        return false;
    }

    result = sqlite3_exec(db_, createDefinitionsTable, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::string errStr = errorMsg ? errorMsg : "unknown error";
        LOG_ERROR("[Persistence] Failed to create definitions table: " + errStr);
        if (errorMsg) sqlite3_free(errorMsg);
        return false;
    }

    result = sqlite3_exec(db_, createIndexOnWorkflowName, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::string errStr = errorMsg ? errorMsg : "unknown error";
        LOG_ERROR("[Persistence] Failed to create index on workflow_name: " + errStr);
        if (errorMsg) sqlite3_free(errorMsg);
        return false;
    }

    result = sqlite3_exec(db_, createIndexOnState, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::string errStr = errorMsg ? errorMsg : "unknown error";
        LOG_ERROR("[Persistence] Failed to create index on state: " + errStr);
        if (errorMsg) sqlite3_free(errorMsg);
        return false;
    }

    result = sqlite3_exec(db_, createIndexOnStartTime, nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::string errStr = errorMsg ? errorMsg : "unknown error";
        LOG_ERROR("[Persistence] Failed to create index on start_time: " + errStr);
        if (errorMsg) sqlite3_free(errorMsg);
        return false;
    }

    return true;
}

bool SqliteWorkflowPersistence::prepareStatements() {
    int result;

    // 保存工作流记录
    result = sqlite3_prepare_v2(db_,
        "INSERT INTO workflows (id, workflow_name, state, start_time, end_time, "
        "duration, success, error_message, retry_count, input_config) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);",
        -1, &stmtSaveWorkflow_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare save workflow statement");
        return false;
    }

    // 根据ID获取工作流
    result = sqlite3_prepare_v2(db_,
        "SELECT id, workflow_name, state, start_time, end_time, duration, "
        "success, error_message, retry_count, input_config "
        "FROM workflows WHERE id = ?1;",
        -1, &stmtGetWorkflowById_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get workflow by id statement");
        return false;
    }

    // 获取所有工作流
    result = sqlite3_prepare_v2(db_,
        "SELECT id, workflow_name, state, start_time, end_time, duration, "
        "success, error_message, retry_count, input_config "
        "FROM workflows ORDER BY start_time DESC;",
        -1, &stmtGetAllWorkflows_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get all workflows statement");
        return false;
    }

    // 根据名称查询
    result = sqlite3_prepare_v2(db_,
        "SELECT id, workflow_name, state, start_time, end_time, duration, "
        "success, error_message, retry_count, input_config "
        "FROM workflows WHERE workflow_name LIKE ?1 ORDER BY start_time DESC;",
        -1, &stmtGetWorkflowsByName_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get workflows by name statement");
        return false;
    }

    // 根据状态查询
    result = sqlite3_prepare_v2(db_,
        "SELECT id, workflow_name, state, start_time, end_time, duration, "
        "success, error_message, retry_count, input_config "
        "FROM workflows WHERE state = ?1 ORDER BY start_time DESC;",
        -1, &stmtGetWorkflowsByState_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get workflows by state statement");
        return false;
    }

    // 根据时间范围查询
    result = sqlite3_prepare_v2(db_,
        "SELECT id, workflow_name, state, start_time, end_time, duration, "
        "success, error_message, retry_count, input_config "
        "FROM workflows WHERE start_time >= ?1 AND end_time <= ?2 ORDER BY start_time DESC;",
        -1, &stmtGetWorkflowsByTimeRange_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get workflows by time range statement");
        return false;
    }

    // 删除工作流
    result = sqlite3_prepare_v2(db_,
        "DELETE FROM workflows WHERE id = ?1;",
        -1, &stmtDeleteWorkflow_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare delete workflow statement");
        return false;
    }

    // 清除所有工作流
    result = sqlite3_prepare_v2(db_,
        "DELETE FROM workflows;",
        -1, &stmtClearAllWorkflows_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare clear workflows statement");
        return false;
    }

    // 获取总数量
    result = sqlite3_prepare_v2(db_,
        "SELECT COUNT(*) FROM workflows;",
        -1, &stmtGetTotalCount_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get total count statement");
        return false;
    }

    // 获取成功数量
    result = sqlite3_prepare_v2(db_,
        "SELECT COUNT(*) FROM workflows WHERE success = 1;",
        -1, &stmtGetSuccessCount_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get success count statement");
        return false;
    }

    // 获取失败数量
    result = sqlite3_prepare_v2(db_,
        "SELECT COUNT(*) FROM workflows WHERE success = 0;",
        -1, &stmtGetFailureCount_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get failure count statement");
        return false;
    }

    // 获取成功率
    result = sqlite3_prepare_v2(db_,
        "SELECT CAST(COUNT(*) * 100.0 AS REAL) / (SELECT COUNT(*) FROM workflows) FROM workflows WHERE success = 1;",
        -1, &stmtGetSuccessRate_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get success rate statement");
        return false;
    }

    // 获取平均耗时
    result = sqlite3_prepare_v2(db_,
        "SELECT AVG(duration) FROM workflows WHERE duration > 0;",
        -1, &stmtGetAverageDuration_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get average duration statement");
        return false;
    }

    // 保存工作流定义
    result = sqlite3_prepare_v2(db_,
        "INSERT OR REPLACE INTO workflow_definitions (workflow_id, definition_json, created_at) "
        "VALUES (?1, ?2, ?3);",
        -1, &stmtSaveDefinition_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare save definition statement");
        return false;
    }

    // 获取工作流定义
    result = sqlite3_prepare_v2(db_,
        "SELECT definition_json FROM workflow_definitions WHERE workflow_id = ?1;",
        -1, &stmtGetDefinition_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get definition statement");
        return false;
    }

    // 获取所有工作流定义
    result = sqlite3_prepare_v2(db_,
        "SELECT workflow_id, definition_json FROM workflow_definitions ORDER BY created_at DESC;",
        -1, &stmtGetAllDefinitions_, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to prepare get all definitions statement");
        return false;
    }

    return true;
}

void SqliteWorkflowPersistence::finalizeStatements() {
    if (stmtSaveWorkflow_) sqlite3_finalize(stmtSaveWorkflow_);
    if (stmtGetWorkflowById_) sqlite3_finalize(stmtGetWorkflowById_);
    if (stmtGetAllWorkflows_) sqlite3_finalize(stmtGetAllWorkflows_);
    if (stmtGetWorkflowsByName_) sqlite3_finalize(stmtGetWorkflowsByName_);
    if (stmtGetWorkflowsByState_) sqlite3_finalize(stmtGetWorkflowsByState_);
    if (stmtGetWorkflowsByTimeRange_) sqlite3_finalize(stmtGetWorkflowsByTimeRange_);
    if (stmtDeleteWorkflow_) sqlite3_finalize(stmtDeleteWorkflow_);
    if (stmtClearAllWorkflows_) sqlite3_finalize(stmtClearAllWorkflows_);

    if (stmtGetTotalCount_) sqlite3_finalize(stmtGetTotalCount_);
    if (stmtGetSuccessCount_) sqlite3_finalize(stmtGetSuccessCount_);
    if (stmtGetFailureCount_) sqlite3_finalize(stmtGetFailureCount_);
    if (stmtGetSuccessRate_) sqlite3_finalize(stmtGetSuccessRate_);
    if (stmtGetAverageDuration_) sqlite3_finalize(stmtGetAverageDuration_);

    if (stmtSaveDefinition_) sqlite3_finalize(stmtSaveDefinition_);
    if (stmtGetDefinition_) sqlite3_finalize(stmtGetDefinition_);
    if (stmtGetAllDefinitions_) sqlite3_finalize(stmtGetAllDefinitions_);

    stmtSaveWorkflow_ = nullptr;
    stmtGetWorkflowById_ = nullptr;
    stmtGetAllWorkflows_ = nullptr;
    stmtGetWorkflowsByName_ = nullptr;
    stmtGetWorkflowsByState_ = nullptr;
    stmtGetWorkflowsByTimeRange_ = nullptr;
    stmtDeleteWorkflow_ = nullptr;
    stmtClearAllWorkflows_ = nullptr;
    stmtGetTotalCount_ = nullptr;
    stmtGetSuccessCount_ = nullptr;
    stmtGetFailureCount_ = nullptr;
    stmtGetSuccessRate_ = nullptr;
    stmtGetAverageDuration_ = nullptr;
    stmtSaveDefinition_ = nullptr;
    stmtGetDefinition_ = nullptr;
    stmtGetAllDefinitions_ = nullptr;
}

std::string SqliteWorkflowPersistence::generateWorkflowId() {
    // 生成唯一的工作流ID（时间戳 + 随机数）
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<> dis(0, 9999);

    return "wf_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

bool SqliteWorkflowPersistence::saveWorkflow(const WorkflowRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return false;
    }

    int result = sqlite3_bind_text(stmtSaveWorkflow_, 1, record.workflowId.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow id");
        return false;
    }

    result = sqlite3_bind_text(stmtSaveWorkflow_, 2, record.workflowName.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow name");
        return false;
    }

    result = sqlite3_bind_int(stmtSaveWorkflow_, 3, static_cast<int>(record.state));
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind state");
        return false;
    }

    result = sqlite3_bind_int64(stmtSaveWorkflow_, 4, record.startTime);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind start time");
        return false;
    }

    result = sqlite3_bind_int64(stmtSaveWorkflow_, 5, record.endTime);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind end time");
        return false;
    }

    result = sqlite3_bind_int64(stmtSaveWorkflow_, 6, record.duration);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind duration");
        return false;
    }

    result = sqlite3_bind_int(stmtSaveWorkflow_, 7, record.success ? 1 : 0);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind success");
        return false;
    }

    const char* errorMsgPtr = record.errorMessage.empty() ? nullptr : record.errorMessage.c_str();
    result = sqlite3_bind_text(stmtSaveWorkflow_, 8, errorMsgPtr, -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind error message");
        return false;
    }

    result = sqlite3_bind_int(stmtSaveWorkflow_, 9, record.retryCount);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind retry count");
        return false;
    }

    const char* inputConfigPtr = record.inputConfig.empty() ? nullptr : record.inputConfig.c_str();
    result = sqlite3_bind_text(stmtSaveWorkflow_, 10, inputConfigPtr, -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind input config");
        return false;
    }

    result = sqlite3_step(stmtSaveWorkflow_);
    if (result != SQLITE_DONE) {
        std::string error = "Failed to execute save workflow statement: ";
        error += std::to_string(result);
        LOG_ERROR("[Persistence] " + error);
        sqlite3_reset(stmtSaveWorkflow_);
        return false;
    }

    sqlite3_reset(stmtSaveWorkflow_);
    LOG_INFO("[Persistence] Workflow saved: " + record.workflowId);
    return true;
}

WorkflowRecord SqliteWorkflowPersistence::getWorkflowById(const std::string& workflowId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return WorkflowRecord();
    }

    int result = sqlite3_bind_text(stmtGetWorkflowById_, 1, workflowId.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow id for get");
        return WorkflowRecord();
    }

    WorkflowRecord record;
    if (sqlite3_step(stmtGetWorkflowById_) == SQLITE_ROW) {
        record.workflowId = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowById_, 0));
        record.workflowName = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowById_, 1));

        record.state = static_cast<WorkflowState>(sqlite3_column_int(stmtGetWorkflowById_, 2));
        record.startTime = sqlite3_column_int64(stmtGetWorkflowById_, 3);
        record.endTime = sqlite3_column_int64(stmtGetWorkflowById_, 4);
        record.duration = sqlite3_column_int64(stmtGetWorkflowById_, 5);
        record.success = sqlite3_column_int(stmtGetWorkflowById_, 6) != 0;

        const char* errorMsg = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowById_, 7));
        record.errorMessage = errorMsg ? errorMsg : "";
        record.retryCount = sqlite3_column_int(stmtGetWorkflowById_, 8);

        const char* inputConfig = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowById_, 9));
        record.inputConfig = inputConfig ? inputConfig : "";
    }

    sqlite3_reset(stmtGetWorkflowById_);
    return record;
}

std::vector<WorkflowRecord> SqliteWorkflowPersistence::getAllWorkflows() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return {};
    }

    std::vector<WorkflowRecord> records;
    while (sqlite3_step(stmtGetAllWorkflows_) == SQLITE_ROW) {
        WorkflowRecord record;
        record.workflowId = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetAllWorkflows_, 0));
        record.workflowName = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetAllWorkflows_, 1));

        record.state = static_cast<WorkflowState>(sqlite3_column_int(stmtGetAllWorkflows_, 2));
        record.startTime = sqlite3_column_int64(stmtGetAllWorkflows_, 3);
        record.endTime = sqlite3_column_int64(stmtGetAllWorkflows_, 4);
        record.duration = sqlite3_column_int64(stmtGetAllWorkflows_, 5);
        record.success = sqlite3_column_int(stmtGetAllWorkflows_, 6) != 0;

        const char* errorMsg = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetAllWorkflows_, 7));
        record.errorMessage = errorMsg ? errorMsg : "";
        record.retryCount = sqlite3_column_int(stmtGetAllWorkflows_, 8);

        const char* inputConfig = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetAllWorkflows_, 9));
        record.inputConfig = inputConfig ? inputConfig : "";

        records.push_back(record);
    }

    sqlite3_reset(stmtGetAllWorkflows_);
    return records;
}

std::vector<WorkflowRecord> SqliteWorkflowPersistence::getWorkflowsByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return {};
    }

    std::string pattern = "%" + name + "%";
    int result = sqlite3_bind_text(stmtGetWorkflowsByName_, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow name pattern");
        return {};
    }

    std::vector<WorkflowRecord> records;
    while (sqlite3_step(stmtGetWorkflowsByName_) == SQLITE_ROW) {
        WorkflowRecord record;
        record.workflowId = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByName_, 0));
        record.workflowName = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByName_, 1));

        record.state = static_cast<WorkflowState>(sqlite3_column_int(stmtGetWorkflowsByName_, 2));
        record.startTime = sqlite3_column_int64(stmtGetWorkflowsByName_, 3);
        record.endTime = sqlite3_column_int64(stmtGetWorkflowsByName_, 4);
        record.duration = sqlite3_column_int64(stmtGetWorkflowsByName_, 5);
        record.success = sqlite3_column_int(stmtGetWorkflowsByName_, 6) != 0;

        const char* errorMsg = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByName_, 7));
        record.errorMessage = errorMsg ? errorMsg : "";
        record.retryCount = sqlite3_column_int(stmtGetWorkflowsByName_, 8);

        const char* inputConfig = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByName_, 9));
        record.inputConfig = inputConfig ? inputConfig : "";

        records.push_back(record);
    }

    sqlite3_reset(stmtGetWorkflowsByName_);
    return records;
}

std::vector<WorkflowRecord> SqliteWorkflowPersistence::getWorkflowsByState(WorkflowState state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return {};
    }

    int result = sqlite3_bind_int(stmtGetWorkflowsByState_, 1, static_cast<int>(state));
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind state");
        return {};
    }

    std::vector<WorkflowRecord> records;
    while (sqlite3_step(stmtGetWorkflowsByState_) == SQLITE_ROW) {
        WorkflowRecord record;
        record.workflowId = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByState_, 0));
        record.workflowName = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByState_, 1));

        record.state = static_cast<WorkflowState>(sqlite3_column_int(stmtGetWorkflowsByState_, 2));
        record.startTime = sqlite3_column_int64(stmtGetWorkflowsByState_, 3);
        record.endTime = sqlite3_column_int64(stmtGetWorkflowsByState_, 4);
        record.duration = sqlite3_column_int64(stmtGetWorkflowsByState_, 5);
        record.success = sqlite3_column_int(stmtGetWorkflowsByState_, 6) != 0;

        const char* errorMsg = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByState_, 7));
        record.errorMessage = errorMsg ? errorMsg : "";
        record.retryCount = sqlite3_column_int(stmtGetWorkflowsByState_, 8);

        const char* inputConfig = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByState_, 9));
        record.inputConfig = inputConfig ? inputConfig : "";

        records.push_back(record);
    }

    sqlite3_reset(stmtGetWorkflowsByState_);
    return records;
}

std::vector<WorkflowRecord> SqliteWorkflowPersistence::getWorkflowsByTimeRange(int64_t startTime, int64_t endTime) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return {};
    }

    int result = sqlite3_bind_int64(stmtGetWorkflowsByTimeRange_, 1, startTime);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind start time");
        return {};
    }

    result = sqlite3_bind_int64(stmtGetWorkflowsByTimeRange_, 2, endTime);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind end time");
        return {};
    }

    std::vector<WorkflowRecord> records;
    while (sqlite3_step(stmtGetWorkflowsByTimeRange_) == SQLITE_ROW) {
        WorkflowRecord record;
        record.workflowId = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByTimeRange_, 0));
        record.workflowName = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByTimeRange_, 1));

        record.state = static_cast<WorkflowState>(sqlite3_column_int(stmtGetWorkflowsByTimeRange_, 2));
        record.startTime = sqlite3_column_int64(stmtGetWorkflowsByTimeRange_, 3);
        record.endTime = sqlite3_column_int64(stmtGetWorkflowsByTimeRange_, 4);
        record.duration = sqlite3_column_int64(stmtGetWorkflowsByTimeRange_, 5);
        record.success = sqlite3_column_int(stmtGetWorkflowsByTimeRange_, 6) != 0;

        const char* errorMsg = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByTimeRange_, 7));
        record.errorMessage = errorMsg ? errorMsg : "";
        record.retryCount = sqlite3_column_int(stmtGetWorkflowsByTimeRange_, 8);

        const char* inputConfig = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetWorkflowsByTimeRange_, 9));
        record.inputConfig = inputConfig ? inputConfig : "";

        records.push_back(record);
    }

    sqlite3_reset(stmtGetWorkflowsByTimeRange_);
    return records;
}

bool SqliteWorkflowPersistence::deleteWorkflow(const std::string& workflowId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return false;
    }

    int result = sqlite3_bind_text(stmtDeleteWorkflow_, 1, workflowId.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow id for delete");
        return false;
    }

    result = sqlite3_step(stmtDeleteWorkflow_);
    if (result != SQLITE_DONE) {
        LOG_ERROR("[Persistence] Failed to execute delete workflow statement");
        sqlite3_reset(stmtDeleteWorkflow_);
        return false;
    }

    sqlite3_reset(stmtDeleteWorkflow_);
    LOG_INFO("[Persistence] Workflow deleted: " + workflowId);
    return true;
}

bool SqliteWorkflowPersistence::clearAllWorkflows() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return false;
    }

    int result = sqlite3_step(stmtClearAllWorkflows_);
    if (result != SQLITE_DONE) {
        LOG_ERROR("[Persistence] Failed to execute clear workflows statement");
        sqlite3_reset(stmtClearAllWorkflows_);
        return false;
    }

    sqlite3_reset(stmtClearAllWorkflows_);
    LOG_INFO("[Persistence] All workflows cleared");
    return true;
}

int SqliteWorkflowPersistence::getTotalWorkflowCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return 0;
    }

    int result = sqlite3_step(stmtGetTotalCount_);
    if (result != SQLITE_ROW) {
        LOG_ERROR("[Persistence] Failed to get total count");
        sqlite3_reset(stmtGetTotalCount_);
        return 0;
    }

    int count = sqlite3_column_int(stmtGetTotalCount_, 0);
    sqlite3_reset(stmtGetTotalCount_);
    return count;
}

int SqliteWorkflowPersistence::getSuccessCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return 0;
    }

    int result = sqlite3_step(stmtGetSuccessCount_);
    if (result != SQLITE_ROW) {
        LOG_ERROR("[Persistence] Failed to get success count");
        sqlite3_reset(stmtGetSuccessCount_);
        return 0;
    }

    int count = sqlite3_column_int(stmtGetSuccessCount_, 0);
    sqlite3_reset(stmtGetSuccessCount_);
    return count;
}

int SqliteWorkflowPersistence::getFailureCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return 0;
    }

    int result = sqlite3_step(stmtGetFailureCount_);
    if (result != SQLITE_ROW) {
        LOG_ERROR("[Persistence] Failed to get failure count");
        sqlite3_reset(stmtGetFailureCount_);
        return 0;
    }

    int count = sqlite3_column_int(stmtGetFailureCount_, 0);
    sqlite3_reset(stmtGetFailureCount_);
    return count;
}

double SqliteWorkflowPersistence::getSuccessRate() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return 0.0;
    }

    int result = sqlite3_step(stmtGetSuccessRate_);
    if (result != SQLITE_ROW) {
        LOG_ERROR("[Persistence] Failed to get success rate");
        sqlite3_reset(stmtGetSuccessRate_);
        return 0.0;
    }

    double successRate = sqlite3_column_double(stmtGetSuccessRate_, 0);
    sqlite3_reset(stmtGetSuccessRate_);
    return successRate;
}

int64_t SqliteWorkflowPersistence::getAverageDuration() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return 0;
    }

    int result = sqlite3_step(stmtGetAverageDuration_);
    if (result != SQLITE_ROW) {
        LOG_ERROR("[Persistence] Failed to get average duration");
        sqlite3_reset(stmtGetAverageDuration_);
        return 0;
    }

    int64_t avgDuration = sqlite3_column_int64(stmtGetAverageDuration_, 0);
    sqlite3_reset(stmtGetAverageDuration_);
    return avgDuration;
}

bool SqliteWorkflowPersistence::saveWorkflowDefinition(const std::string& workflowId,
                                                    const std::string& workflowJson) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    int result = sqlite3_bind_text(stmtSaveDefinition_, 1, workflowId.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow id for definition");
        return false;
    }

    result = sqlite3_bind_text(stmtSaveDefinition_, 2, workflowJson.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind definition json");
        return false;
    }

    result = sqlite3_bind_int64(stmtSaveDefinition_, 3, static_cast<int64_t>(timestamp));
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind timestamp");
        return false;
    }

    result = sqlite3_step(stmtSaveDefinition_);
    if (result != SQLITE_DONE) {
        std::string error = "Failed to execute save definition statement: ";
        error += std::to_string(result);
        LOG_ERROR("[Persistence] " + error);
        sqlite3_reset(stmtSaveDefinition_);
        return false;
    }

    sqlite3_reset(stmtSaveDefinition_);
    LOG_INFO("[Persistence] Workflow definition saved: " + workflowId);
    return true;
}

std::string SqliteWorkflowPersistence::getWorkflowDefinition(const std::string& workflowId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return "";
    }

    int result = sqlite3_bind_text(stmtGetDefinition_, 1, workflowId.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        LOG_ERROR("[Persistence] Failed to bind workflow id for get definition");
        return "";
    }

    std::string definitionJson = "";
    if (sqlite3_step(stmtGetDefinition_) == SQLITE_ROW) {
        definitionJson = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetDefinition_, 0));
    }

    sqlite3_reset(stmtGetDefinition_);
    return definitionJson;
}

std::map<std::string, std::string> SqliteWorkflowPersistence::getAllWorkflowDefinitions() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("[Persistence] Not initialized");
        return {};
    }

    std::map<std::string, std::string> definitions;
    while (sqlite3_step(stmtGetAllDefinitions_) == SQLITE_ROW) {
        std::string workflowId = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetAllDefinitions_, 0));
        std::string definitionJson = reinterpret_cast<const char*>(sqlite3_column_text(stmtGetAllDefinitions_, 1));

        definitions[workflowId] = definitionJson;
    }

    sqlite3_reset(stmtGetAllDefinitions_);
    return definitions;
}

} // namespace WorkflowSystem
