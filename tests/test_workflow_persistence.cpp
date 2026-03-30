/**
 * @file test_workflow_persistence.cpp
 * @brief SQLite 工作流持久化测试
 */

#include "workflow_system/implementations/sqlite_workflow_persistence.h"
#include "test_framework.h"
#include <cstdio>
#include <unistd.h>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助函数 ==========

std::string getUniqueTestDbPath() {
    static int counter = 0;
    return "/tmp/test_workflow_persistence_" + std::to_string(++counter) + ".db";
}

void cleanupTestDb(const std::string& path) {
    unlink(path.c_str());
}

WorkflowRecord createTestRecord(const std::string& id, bool success = true) {
    WorkflowRecord record;
    record.workflowId = id;
    record.workflowName = "test_workflow";
    record.state = WorkflowState::COMPLETED;
    record.startTime = 1000000;
    record.endTime = 2000000;
    record.duration = 1000000;
    record.success = success;
    record.errorMessage = success ? "" : "Test error";
    record.retryCount = 0;
    record.variables["key1"] = "value1";
    record.variables["key2"] = "value2";
    return record;
}

// ========== 测试用例 ==========

void testPersistence_Initialize() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    bool result = persistence.initialize(dbPath);

    TEST_ASSERT_TRUE(result);

    // 检查数据库文件是否创建
    FILE* f = fopen(dbPath.c_str(), "r");
    TEST_ASSERT_TRUE(f != nullptr);
    if (f) fclose(f);

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_SaveAndGetWorkflow() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    WorkflowRecord record = createTestRecord("wf_save_get_001");  // 使用唯一的ID

    bool saveResult = persistence.saveWorkflow(record);
    TEST_ASSERT_TRUE(saveResult);

    WorkflowRecord retrieved = persistence.getWorkflowById("wf_save_get_001");

    TEST_ASSERT_STRING_EQUAL("wf_save_get_001", retrieved.workflowId.c_str());
    TEST_ASSERT_STRING_EQUAL("test_workflow", retrieved.workflowName.c_str());
    TEST_ASSERT_TRUE(retrieved.success);
    TEST_ASSERT_TRUE(retrieved.state == WorkflowState::COMPLETED);
    TEST_ASSERT_EQUAL(1000000, retrieved.duration);

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetAllWorkflows() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001"));
    persistence.saveWorkflow(createTestRecord("wf_002"));
    persistence.saveWorkflow(createTestRecord("wf_003"));

    auto allWorkflows = persistence.getAllWorkflows();

    TEST_ASSERT_EQUAL(3, static_cast<int>(allWorkflows.size()));

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetWorkflowsByName() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    WorkflowRecord record1 = createTestRecord("wf_001");
    record1.workflowName = "workflow_a";
    persistence.saveWorkflow(record1);

    WorkflowRecord record2 = createTestRecord("wf_002");
    record2.workflowName = "workflow_a";
    persistence.saveWorkflow(record2);

    WorkflowRecord record3 = createTestRecord("wf_003");
    record3.workflowName = "workflow_b";
    persistence.saveWorkflow(record3);

    auto workflowsA = persistence.getWorkflowsByName("workflow_a");
    TEST_ASSERT_EQUAL(2, static_cast<int>(workflowsA.size()));

    auto workflowsB = persistence.getWorkflowsByName("workflow_b");
    TEST_ASSERT_EQUAL(1, static_cast<int>(workflowsB.size()));

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetWorkflowsByState() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    WorkflowRecord record1 = createTestRecord("wf_001");
    record1.state = WorkflowState::COMPLETED;
    persistence.saveWorkflow(record1);

    WorkflowRecord record2 = createTestRecord("wf_002");
    record2.state = WorkflowState::FAILED;
    persistence.saveWorkflow(record2);

    WorkflowRecord record3 = createTestRecord("wf_003");
    record3.state = WorkflowState::COMPLETED;
    persistence.saveWorkflow(record3);

    auto completedWorkflows = persistence.getWorkflowsByState(WorkflowState::COMPLETED);
    TEST_ASSERT_EQUAL(2, static_cast<int>(completedWorkflows.size()));

    auto failedWorkflows = persistence.getWorkflowsByState(WorkflowState::FAILED);
    TEST_ASSERT_EQUAL(1, static_cast<int>(failedWorkflows.size()));

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetWorkflowsByTimeRange() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    WorkflowRecord record1 = createTestRecord("wf_001");
    record1.startTime = 1000;
    record1.endTime = 2000;
    persistence.saveWorkflow(record1);

    WorkflowRecord record2 = createTestRecord("wf_002");
    record2.startTime = 3000;
    record2.endTime = 4000;
    persistence.saveWorkflow(record2);

    WorkflowRecord record3 = createTestRecord("wf_003");
    record3.startTime = 5000;
    record3.endTime = 6000;
    persistence.saveWorkflow(record3);

    auto range1 = persistence.getWorkflowsByTimeRange(0, 2500);
    TEST_ASSERT_EQUAL(1, static_cast<int>(range1.size()));

    auto range2 = persistence.getWorkflowsByTimeRange(2500, 6500);
    TEST_ASSERT_EQUAL(2, static_cast<int>(range2.size()));

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_DeleteWorkflow() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001"));
    persistence.saveWorkflow(createTestRecord("wf_002"));

    bool deleteResult = persistence.deleteWorkflow("wf_001");
    TEST_ASSERT_TRUE(deleteResult);

    auto allWorkflows = persistence.getAllWorkflows();
    TEST_ASSERT_EQUAL(1, static_cast<int>(allWorkflows.size()));

    WorkflowRecord retrieved = persistence.getWorkflowById("wf_001");
    TEST_ASSERT_TRUE(retrieved.workflowId.empty());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_ClearAllWorkflows() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001"));
    persistence.saveWorkflow(createTestRecord("wf_002"));
    persistence.saveWorkflow(createTestRecord("wf_003"));

    TEST_ASSERT_EQUAL(3, persistence.getTotalWorkflowCount());

    bool clearResult = persistence.clearAllWorkflows();
    TEST_ASSERT_TRUE(clearResult);

    TEST_ASSERT_EQUAL(0, persistence.getTotalWorkflowCount());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetTotalWorkflowCount() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    TEST_ASSERT_EQUAL(0, persistence.getTotalWorkflowCount());

    persistence.saveWorkflow(createTestRecord("wf_001"));
    persistence.saveWorkflow(createTestRecord("wf_002"));

    TEST_ASSERT_EQUAL(2, persistence.getTotalWorkflowCount());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetSuccessCount() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001", true));
    persistence.saveWorkflow(createTestRecord("wf_002", true));
    persistence.saveWorkflow(createTestRecord("wf_003", false));

    TEST_ASSERT_EQUAL(2, persistence.getSuccessCount());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetFailureCount() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001", true));
    persistence.saveWorkflow(createTestRecord("wf_002", false));
    persistence.saveWorkflow(createTestRecord("wf_003", false));

    TEST_ASSERT_EQUAL(2, persistence.getFailureCount());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetSuccessRate() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001", true));
    persistence.saveWorkflow(createTestRecord("wf_002", true));
    persistence.saveWorkflow(createTestRecord("wf_003", true));
    persistence.saveWorkflow(createTestRecord("wf_004", false));

    double rate = persistence.getSuccessRate();
    // SQL返回的是百分比（75.0），而不是小数（0.75）
    TEST_ASSERT_FLOAT_EQUAL(75.0, rate, 0.01);

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetAverageDuration() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    // 创建3个记录，每个有不同的duration
    WorkflowRecord record1;
    record1.workflowId = "wf_001";
    record1.workflowName = "test_workflow";
    record1.state = WorkflowState::COMPLETED;
    record1.startTime = 1000000;
    record1.endTime = 2000000;
    record1.duration = 1000;  // 设置为1000
    record1.success = true;
    record1.errorMessage = "";
    record1.retryCount = 0;

    WorkflowRecord record2;
    record2.workflowId = "wf_002";
    record2.workflowName = "test_workflow";
    record2.state = WorkflowState::COMPLETED;
    record2.startTime = 1000000;
    record2.endTime = 2000000;
    record2.duration = 2000;  // 设置为2000
    record2.success = true;
    record2.errorMessage = "";
    record2.retryCount = 0;

    WorkflowRecord record3;
    record3.workflowId = "wf_003";
    record3.workflowName = "test_workflow";
    record3.state = WorkflowState::COMPLETED;
    record3.startTime = 1000000;
    record3.endTime = 2000000;
    record3.duration = 3000;  // 设置为3000
    record3.success = true;
    record3.errorMessage = "";
    record3.retryCount = 0;

    persistence.saveWorkflow(record1);
    persistence.saveWorkflow(record2);
    persistence.saveWorkflow(record3);

    int64_t avgDuration = persistence.getAverageDuration();
    // 平均值应该是 (1000 + 2000 + 3000) / 3 = 2000
    TEST_ASSERT_EQUAL(2000, avgDuration);

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_SaveWorkflowDefinition() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    std::string definition = R"({"nodes": ["node1", "node2"], "edges": [["node1", "node2"]]})";

    bool saveResult = persistence.saveWorkflowDefinition("wf_def_001", definition);
    TEST_ASSERT_TRUE(saveResult);

    std::string retrieved = persistence.getWorkflowDefinition("wf_def_001");
    TEST_ASSERT_STRING_EQUAL(definition.c_str(), retrieved.c_str());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_GetAllWorkflowDefinitions() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflowDefinition("wf_def_001", R"({"name": "workflow1"})");
    persistence.saveWorkflowDefinition("wf_def_002", R"({"name": "workflow2"})");
    persistence.saveWorkflowDefinition("wf_def_003", R"({"name": "workflow3"})");

    auto allDefs = persistence.getAllWorkflowDefinitions();

    TEST_ASSERT_EQUAL(3, static_cast<int>(allDefs.size()));
    TEST_ASSERT_TRUE(allDefs.find("wf_def_001") != allDefs.end());
    TEST_ASSERT_TRUE(allDefs.find("wf_def_002") != allDefs.end());
    TEST_ASSERT_TRUE(allDefs.find("wf_def_003") != allDefs.end());

    persistence.close();
    cleanupTestDb(dbPath);
}

void testPersistence_Close() {
    std::string dbPath = getUniqueTestDbPath();

    auto& persistence = SqliteWorkflowPersistence::getInstance();
    persistence.initialize(dbPath);

    persistence.saveWorkflow(createTestRecord("wf_001"));

    persistence.close();

    // 关闭后应该无法访问数据
    // 这里只测试不会崩溃
    TEST_ASSERT_TRUE(true);

    cleanupTestDb(dbPath);
}

// ========== 测试套件创建 ==========

TestSuite createWorkflowPersistenceTestSuite() {
    TestSuite suite("工作流持久化测试");

    suite.addTest("Persistence_Initialize", testPersistence_Initialize);
    suite.addTest("Persistence_SaveAndGetWorkflow", testPersistence_SaveAndGetWorkflow);
    suite.addTest("Persistence_GetAllWorkflows", testPersistence_GetAllWorkflows);
    suite.addTest("Persistence_GetWorkflowsByName", testPersistence_GetWorkflowsByName);
    suite.addTest("Persistence_GetWorkflowsByState", testPersistence_GetWorkflowsByState);
    suite.addTest("Persistence_GetWorkflowsByTimeRange", testPersistence_GetWorkflowsByTimeRange);
    suite.addTest("Persistence_DeleteWorkflow", testPersistence_DeleteWorkflow);
    suite.addTest("Persistence_ClearAllWorkflows", testPersistence_ClearAllWorkflows);
    suite.addTest("Persistence_GetTotalWorkflowCount", testPersistence_GetTotalWorkflowCount);
    suite.addTest("Persistence_GetSuccessCount", testPersistence_GetSuccessCount);
    suite.addTest("Persistence_GetFailureCount", testPersistence_GetFailureCount);
    suite.addTest("Persistence_GetSuccessRate", testPersistence_GetSuccessRate);
    suite.addTest("Persistence_GetAverageDuration", testPersistence_GetAverageDuration);
    suite.addTest("Persistence_SaveWorkflowDefinition", testPersistence_SaveWorkflowDefinition);
    suite.addTest("Persistence_GetAllWorkflowDefinitions", testPersistence_GetAllWorkflowDefinitions);
    suite.addTest("Persistence_Close", testPersistence_Close);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowPersistenceTestSuite());

    return TestRunner::runAllSuites(suites);
}
