/**
 * @file test_all.cpp
 * @brief 所有模块的综合测试
 */

#include "core/types.h"
#include "core/any.h"
#include "core/utils.h"
#include "core/logger.h"
#include "implementations/workflow_context.h"
#include "implementations/filesystem_resource_manager.h"
#include "implementations/circuit_breaker.h"
#include "implementations/checkpoint_manager_impl.h"
#include "test_framework.h"
#include <vector>
#include <map>
#include <thread>
#include <chrono>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== Any 类型测试 ==========
void testAny_Empty() {
    Any a;
    TEST_ASSERT_TRUE(a.isEmpty());
    TEST_ASSERT_FALSE(a.has_value());
}

void testAny_IntValue() {
    Any a(42);
    TEST_ASSERT_FALSE(a.isEmpty());
    TEST_ASSERT_EQUAL(42, a.cast<int>());
}

void testAny_StringValue() {
    Any a(std::string("hello"));
    TEST_ASSERT_FALSE(a.isEmpty());
    TEST_ASSERT_STRING_EQUAL("hello", a.cast<std::string>());
}

void testAny_DoubleValue() {
    Any a(3.14159);
    TEST_ASSERT_FALSE(a.isEmpty());
    TEST_ASSERT_FLOAT_EQUAL(3.14159, a.cast<double>(), 0.00001);
}

void testAny_Copy() {
    Any a1(100);
    Any a2 = a1;
    TEST_ASSERT_EQUAL(100, a2.cast<int>());
    TEST_ASSERT_FALSE(a2.isEmpty());
}

void testAny_Assignment() {
    Any a1(50);
    Any a2;
    a2 = a1;
    TEST_ASSERT_EQUAL(50, a2.cast<int>());
}

void testAny_Reset() {
    Any a(42);
    TEST_ASSERT_FALSE(a.isEmpty());
    a.reset();
    TEST_ASSERT_TRUE(a.isEmpty());
}

void testAny_Swap() {
    Any a1(10);
    Any a2(20);
    a1.swap(a2);
    TEST_ASSERT_EQUAL(20, a1.cast<int>());
    TEST_ASSERT_EQUAL(10, a2.cast<int>());
}

void testAny_VectorValue() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    Any a(vec);
    auto retrieved = a.cast<std::vector<int>>();
    TEST_ASSERT_EQUAL(5, static_cast<int>(retrieved.size()));
}

// ========== IdGenerator 测试 ==========
void testIdGenerator_GenerateUnique() {
    std::string id1 = IdGenerator::generate();
    std::string id2 = IdGenerator::generate();

    TEST_ASSERT_FALSE(id1.empty());
    TEST_ASSERT_FALSE(id2.empty());
    TEST_ASSERT_NOT_EQUAL(id1, id2);
}

void testIdGenerator_Format() {
    std::string id = IdGenerator::generate();
    TEST_ASSERT_TRUE(id.find("id_") == 0);
}

// ========== TimeUtils 测试 ==========
void testTimeUtils_GetCurrentMs() {
    int64_t t1 = TimeUtils::getCurrentMs();
    TEST_ASSERT_TRUE(t1 > 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t t2 = TimeUtils::getCurrentMs();
    TEST_ASSERT_TRUE(t2 >= t1);
}

void testTimeUtils_GetCurrentTimestamp() {
    std::string ts = TimeUtils::getCurrentTimestamp();
    TEST_ASSERT_FALSE(ts.empty());
    TEST_ASSERT_EQUAL(15, static_cast<int>(ts.length()));
}

void testTimeUtils_FormatDuration() {
    std::string result0 = TimeUtils::formatDuration(0);
    TEST_ASSERT_TRUE(result0 == "0ms" || result0.find("0") != std::string::npos);
    
    std::string result100 = TimeUtils::formatDuration(100);
    TEST_ASSERT_TRUE(result100.find("100ms") != std::string::npos);
    
    std::string result1500 = TimeUtils::formatDuration(1500);
    TEST_ASSERT_TRUE(result1500.find("1s") != std::string::npos || result1500.find("500ms") != std::string::npos);
}

void testTimeUtils_FormatIso8601() {
    std::string iso = TimeUtils::formatIso8601();
    TEST_ASSERT_FALSE(iso.empty());
    TEST_ASSERT_EQUAL(19, static_cast<int>(iso.length()));
}

void testTimeUtils_DiffMs() {
    int64_t diff = TimeUtils::diffMs(1000, 2000);
    TEST_ASSERT_EQUAL(1000, diff);
}

// ========== StringUtils 测试 ==========
void testStringUtils_Contains() {
    TEST_ASSERT_TRUE(StringUtils::contains("hello world", "world"));
    TEST_ASSERT_TRUE(StringUtils::contains("hello world", "hello"));
    TEST_ASSERT_FALSE(StringUtils::contains("hello world", "xyz"));
    TEST_ASSERT_TRUE(StringUtils::contains("hello", ""));
}

void testStringUtils_JoinPath() {
    TEST_ASSERT_STRING_EQUAL("/base/relative", StringUtils::joinPath("/base", "relative"));
    TEST_ASSERT_STRING_EQUAL("/base/relative", StringUtils::joinPath("/base/", "relative"));
    TEST_ASSERT_STRING_EQUAL("relative", StringUtils::joinPath("", "relative"));
    TEST_ASSERT_STRING_EQUAL("/base", StringUtils::joinPath("/base", ""));
}

// ========== WorkflowState 测试 ==========
void testWorkflowState_EnumValues() {
    TEST_ASSERT_EQUAL(0, static_cast<int>(WorkflowState::IDLE));
    TEST_ASSERT_EQUAL(1, static_cast<int>(WorkflowState::INITIALIZING));
    TEST_ASSERT_EQUAL(4, static_cast<int>(WorkflowState::RUNNING));
    TEST_ASSERT_EQUAL(10, static_cast<int>(WorkflowState::COMPLETED));
}

void testWorkflowState_StringConversion() {
    TEST_ASSERT_STRING_EQUAL("idle", workflowStateToString(WorkflowState::IDLE));
    TEST_ASSERT_STRING_EQUAL("running", workflowStateToString(WorkflowState::RUNNING));
    TEST_ASSERT_STRING_EQUAL("completed", workflowStateToString(WorkflowState::COMPLETED));
    TEST_ASSERT_STRING_EQUAL("error", workflowStateToString(WorkflowState::ERROR));
}

// ========== ResourceType 测试 ==========
void testResourceType_StringConversion() {
    TEST_ASSERT_STRING_EQUAL("directory", resourceTypeToString(ResourceType::DIRECTORY));
    TEST_ASSERT_STRING_EQUAL("file", resourceTypeToString(ResourceType::FILE));
    TEST_ASSERT_STRING_EQUAL("temporary", resourceTypeToString(ResourceType::TEMPORARY));
}

// ========== WorkflowContext 测试 ==========
void testContext_SetGetString() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    ctx.set("key1", "value1");
    ctx.set("key2", "value2");
    
    TEST_ASSERT_STRING_EQUAL("value1", ctx.get("key1", ""));
    TEST_ASSERT_STRING_EQUAL("value2", ctx.get("key2", ""));
}

void testContext_GetWithDefault() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    std::string result = ctx.get("nonexistent", "default_value");
    TEST_ASSERT_STRING_EQUAL("default_value", result);
}

void testContext_Has() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    TEST_ASSERT_FALSE(ctx.has("key1"));
    
    ctx.set("key1", "value1");
    TEST_ASSERT_TRUE(ctx.has("key1"));
}

void testContext_SetGetObject() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    ctx.setObject("int_val", 42);
    ctx.setObject("str_val", std::string("hello"));
    ctx.setObject("double_val", 3.14);
    
    TEST_ASSERT_EQUAL(42, ctx.getObject<int>("int_val"));
    TEST_ASSERT_STRING_EQUAL("hello", ctx.getObject<std::string>("str_val"));
    TEST_ASSERT_FLOAT_EQUAL(3.14, ctx.getObject<double>("double_val"), 0.001);
}

void testContext_GetObjectWithDefault() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    int result = ctx.getObject<int>("nonexistent", -1);
    TEST_ASSERT_EQUAL(-1, result);
}

void testContext_SetObjectComplexType() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    ctx.setObject("numbers", numbers);
    
    auto retrieved = ctx.getObject<std::vector<int>>("numbers");
    TEST_ASSERT_EQUAL(5, static_cast<int>(retrieved.size()));
    TEST_ASSERT_EQUAL(1, retrieved[0]);
    TEST_ASSERT_EQUAL(5, retrieved[4]);
}

void testContext_GetAllData() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    WorkflowContext ctx(rm);
    
    ctx.set("key1", "value1");
    ctx.set("key2", "value2");
    
    auto allData = ctx.getAllData();
    TEST_ASSERT_TRUE(allData.size() >= 2);
}

void testContext_ThreadSafety() {
    auto rm = std::make_shared<FileSystemResourceManager>();
    auto ctx = std::make_shared<WorkflowContext>(rm);
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([ctx, i]() {
            for (int j = 0; j < 100; ++j) {
                ctx->set("key_" + std::to_string(i), "value_" + std::to_string(j));
                ctx->get("key_" + std::to_string(i), "");
                ctx->setObject("obj_" + std::to_string(i), j);
                ctx->getObject<int>("obj_" + std::to_string(i), -1);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    TEST_ASSERT_TRUE(true);
}

// ========== Logger 测试 ==========
void testLogger_SetLevel() {
    Logger::getInstance().setLevel(LogLevel::ERROR);
    TEST_ASSERT_TRUE(Logger::getInstance().getLevel() == LogLevel::ERROR);
    
    Logger::getInstance().setLevel(LogLevel::INFO);
    TEST_ASSERT_TRUE(Logger::getInstance().getLevel() == LogLevel::INFO);
}

void testLogger_Singleton() {
    Logger& l1 = Logger::getInstance();
    Logger& l2 = Logger::getInstance();
    TEST_ASSERT_TRUE(&l1 == &l2);
}

// ========== CircuitBreaker 测试 ==========
void testCircuitBreaker_InitialState() {
    CircuitBreaker cb;
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::CLOSED);
    TEST_ASSERT_TRUE(cb.allowRequest());
}

void testCircuitBreaker_ConfigConstructor() {
    CircuitBreakerConfig config;
    config.failureThreshold = 3;
    config.resetTimeoutMs = 1000;
    
    CircuitBreaker cb(config);
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::CLOSED);
    TEST_ASSERT_TRUE(cb.allowRequest());
}

void testCircuitBreaker_RecordSuccess() {
    CircuitBreaker cb;
    cb.recordSuccess();
    
    CircuitBreakerStats stats = cb.getStats();
    TEST_ASSERT_EQUAL(1, stats.successCount);
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::CLOSED);
}

void testCircuitBreaker_OpenAfterFailures() {
    CircuitBreakerConfig config;
    config.failureThreshold = 3;
    CircuitBreaker cb(config);
    
    cb.recordFailure();
    cb.recordFailure();
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::CLOSED);
    
    cb.recordFailure();
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::OPEN);
    TEST_ASSERT_FALSE(cb.allowRequest());
}

void testCircuitBreaker_Reset() {
    CircuitBreakerConfig config;
    config.failureThreshold = 2;
    CircuitBreaker cb(config);
    
    cb.recordFailure();
    cb.recordFailure();
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::OPEN);
    
    cb.reset();
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::CLOSED);
    TEST_ASSERT_TRUE(cb.allowRequest());
}

void testCircuitBreaker_ForceOpenClose() {
    CircuitBreaker cb;
    
    cb.forceOpen();
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::OPEN);
    
    cb.forceClose();
    TEST_ASSERT_TRUE(cb.getState() == CircuitState::CLOSED);
}

void testCircuitBreaker_StateChangeCallback() {
    CircuitBreaker cb;
    bool callbackCalled = false;
    CircuitState oldState = CircuitState::CLOSED;
    CircuitState newState = CircuitState::CLOSED;
    
    cb.setStateChangeCallback([&](CircuitState o, CircuitState n) {
        callbackCalled = true;
        oldState = o;
        newState = n;
    });
    
    cb.forceOpen();
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_TRUE(oldState == CircuitState::CLOSED);
    TEST_ASSERT_TRUE(newState == CircuitState::OPEN);
}

// ========== CheckpointManager 测试 ==========
void testCheckpointManager_CreateCheckpoint() {
    CheckpointManager cm;
    
    std::string cpId = cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL);
    TEST_ASSERT_FALSE(cpId.empty());
    TEST_ASSERT_TRUE(cpId.find("cp_") == 0);
    
    Checkpoint cp = cm.getCheckpoint(cpId);
    TEST_ASSERT_STRING_EQUAL("test_workflow", cp.workflowName);
    TEST_ASSERT_STRING_EQUAL("exec_001", cp.executionId);
    TEST_ASSERT_TRUE(cp.type == CheckpointType::MANUAL);
    TEST_ASSERT_TRUE(cp.status == CheckpointStatus::VALID);
}

void testCheckpointManager_GetCheckpointsByWorkflow() {
    CheckpointManager cm;
    
    cm.createCheckpoint("workflow_a", "exec_001", CheckpointType::MANUAL);
    cm.createCheckpoint("workflow_a", "exec_002", CheckpointType::AUTOMATIC);
    cm.createCheckpoint("workflow_b", "exec_003", CheckpointType::MANUAL);
    
    auto checkpointsA = cm.getCheckpointsByWorkflow("workflow_a");
    TEST_ASSERT_EQUAL(2, static_cast<int>(checkpointsA.size()));
    
    auto checkpointsB = cm.getCheckpointsByWorkflow("workflow_b");
    TEST_ASSERT_EQUAL(1, static_cast<int>(checkpointsB.size()));
    
    auto checkpointsC = cm.getCheckpointsByWorkflow("workflow_c");
    TEST_ASSERT_EQUAL(0, static_cast<int>(checkpointsC.size()));
}

void testCheckpointManager_DeleteCheckpoint() {
    CheckpointManager cm;
    
    std::string cpId = cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL);
    TEST_ASSERT_EQUAL(1, static_cast<int>(cm.getCheckpointCount()));
    
    bool result = cm.deleteCheckpoint(cpId);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, static_cast<int>(cm.getCheckpointCount()));
    
    Checkpoint cp = cm.getCheckpoint(cpId);
    TEST_ASSERT_TRUE(cp.checkpointId.empty());
}

void testCheckpointManager_ValidateCheckpoint() {
    CheckpointManager cm;
    
    std::string cpId = cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL);
    TEST_ASSERT_TRUE(cm.validateCheckpoint(cpId));
    TEST_ASSERT_FALSE(cm.validateCheckpoint("nonexistent"));
}

void testCheckpointManager_HasRestorableCheckpoint() {
    CheckpointManager cm;
    
    TEST_ASSERT_FALSE(cm.hasRestorableCheckpoint("test_workflow"));
    
    cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL);
    TEST_ASSERT_TRUE(cm.hasRestorableCheckpoint("test_workflow"));
}

void testCheckpointManager_ExportImport() {
    CheckpointManager cm;
    
    std::string cpId = cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL);
    
    std::string exported = cm.exportCheckpoint(cpId);
    TEST_ASSERT_FALSE(exported.empty());
    TEST_ASSERT_TRUE(exported.find("test_workflow") != std::string::npos);
    
    std::string newId = cm.importCheckpoint(exported);
    TEST_ASSERT_FALSE(newId.empty());
    
    Checkpoint importedCp = cm.getCheckpoint(newId);
    TEST_ASSERT_STRING_EQUAL("test_workflow", importedCp.workflowName);
    TEST_ASSERT_STRING_EQUAL("exec_001", importedCp.executionId);
}

void testCheckpointManager_ContextData() {
    CheckpointManager cm;
    
    std::unordered_map<std::string, std::string> contextData;
    contextData["key1"] = "value1";
    contextData["key2"] = "value2";
    
    std::string cpId = cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL, contextData);
    
    Checkpoint cp = cm.getCheckpoint(cpId);
    TEST_ASSERT_TRUE(cp.contextData.size() >= 2);
    TEST_ASSERT_STRING_EQUAL("value1", cp.contextData["key1"]);
    TEST_ASSERT_STRING_EQUAL("value2", cp.contextData["key2"]);
}

void testCheckpointManager_UpdateCheckpoint() {
    CheckpointManager cm;
    
    std::string cpId = cm.createCheckpoint("test_workflow", "exec_001", CheckpointType::MANUAL);
    
    std::unordered_map<std::string, std::string> newData;
    newData["new_key"] = "new_value";
    
    bool result = cm.updateCheckpoint(cpId, newData);
    TEST_ASSERT_TRUE(result);
    
    Checkpoint cp = cm.getCheckpoint(cpId);
    TEST_ASSERT_STRING_EQUAL("new_value", cp.contextData["new_key"]);
}

void testCheckpointManager_Config() {
    CheckpointManager cm;
    
    CheckpointConfig config;
    config.maxCheckpoints = 50;
    config.checkpointExpirationMs = 3600000;
    
    cm.setConfig(config);
    
    CheckpointConfig retrieved = cm.getConfig();
    TEST_ASSERT_EQUAL(50, retrieved.maxCheckpoints);
    TEST_ASSERT_EQUAL(3600000, static_cast<int>(retrieved.checkpointExpirationMs));
}

// ========== 测试套件创建 ==========
TestSuite createCoreTypesTestSuite() {
    TestSuite suite("核心类型测试");

    suite.addTest("Any_Empty", testAny_Empty);
    suite.addTest("Any_IntValue", testAny_IntValue);
    suite.addTest("Any_StringValue", testAny_StringValue);
    suite.addTest("Any_DoubleValue", testAny_DoubleValue);
    suite.addTest("Any_Copy", testAny_Copy);
    suite.addTest("Any_Assignment", testAny_Assignment);
    suite.addTest("Any_Reset", testAny_Reset);
    suite.addTest("Any_Swap", testAny_Swap);
    suite.addTest("Any_VectorValue", testAny_VectorValue);

    suite.addTest("IdGenerator_GenerateUnique", testIdGenerator_GenerateUnique);
    suite.addTest("IdGenerator_Format", testIdGenerator_Format);

    suite.addTest("TimeUtils_GetCurrentMs", testTimeUtils_GetCurrentMs);
    suite.addTest("TimeUtils_GetCurrentTimestamp", testTimeUtils_GetCurrentTimestamp);
    suite.addTest("TimeUtils_FormatDuration", testTimeUtils_FormatDuration);
    suite.addTest("TimeUtils_FormatIso8601", testTimeUtils_FormatIso8601);
    suite.addTest("TimeUtils_DiffMs", testTimeUtils_DiffMs);

    suite.addTest("StringUtils_Contains", testStringUtils_Contains);
    suite.addTest("StringUtils_JoinPath", testStringUtils_JoinPath);

    suite.addTest("WorkflowState_EnumValues", testWorkflowState_EnumValues);
    suite.addTest("WorkflowState_StringConversion", testWorkflowState_StringConversion);

    suite.addTest("ResourceType_StringConversion", testResourceType_StringConversion);

    return suite;
}

TestSuite createWorkflowContextTestSuite() {
    TestSuite suite("工作流上下文测试");

    suite.addTest("Context_SetGetString", testContext_SetGetString);
    suite.addTest("Context_GetWithDefault", testContext_GetWithDefault);
    suite.addTest("Context_Has", testContext_Has);
    suite.addTest("Context_SetGetObject", testContext_SetGetObject);
    suite.addTest("Context_GetObjectWithDefault", testContext_GetObjectWithDefault);
    suite.addTest("Context_SetObjectComplexType", testContext_SetObjectComplexType);
    suite.addTest("Context_GetAllData", testContext_GetAllData);
    suite.addTest("Context_ThreadSafety", testContext_ThreadSafety);

    return suite;
}

TestSuite createLoggerTestSuite() {
    TestSuite suite("日志系统测试");

    suite.addTest("Logger_SetLevel", testLogger_SetLevel);
    suite.addTest("Logger_Singleton", testLogger_Singleton);

    return suite;
}

TestSuite createCircuitBreakerTestSuite() {
    TestSuite suite("熔断器测试");

    suite.addTest("CircuitBreaker_InitialState", testCircuitBreaker_InitialState);
    suite.addTest("CircuitBreaker_ConfigConstructor", testCircuitBreaker_ConfigConstructor);
    suite.addTest("CircuitBreaker_RecordSuccess", testCircuitBreaker_RecordSuccess);
    suite.addTest("CircuitBreaker_OpenAfterFailures", testCircuitBreaker_OpenAfterFailures);
    suite.addTest("CircuitBreaker_Reset", testCircuitBreaker_Reset);
    suite.addTest("CircuitBreaker_ForceOpenClose", testCircuitBreaker_ForceOpenClose);
    suite.addTest("CircuitBreaker_StateChangeCallback", testCircuitBreaker_StateChangeCallback);

    return suite;
}

TestSuite createCheckpointManagerTestSuite() {
    TestSuite suite("检查点管理器测试");

    suite.addTest("CheckpointManager_CreateCheckpoint", testCheckpointManager_CreateCheckpoint);
    suite.addTest("CheckpointManager_GetCheckpointsByWorkflow", testCheckpointManager_GetCheckpointsByWorkflow);
    suite.addTest("CheckpointManager_DeleteCheckpoint", testCheckpointManager_DeleteCheckpoint);
    suite.addTest("CheckpointManager_ValidateCheckpoint", testCheckpointManager_ValidateCheckpoint);
    suite.addTest("CheckpointManager_HasRestorableCheckpoint", testCheckpointManager_HasRestorableCheckpoint);
    suite.addTest("CheckpointManager_ExportImport", testCheckpointManager_ExportImport);
    suite.addTest("CheckpointManager_ContextData", testCheckpointManager_ContextData);
    suite.addTest("CheckpointManager_UpdateCheckpoint", testCheckpointManager_UpdateCheckpoint);
    suite.addTest("CheckpointManager_Config", testCheckpointManager_Config);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createCoreTypesTestSuite());
    suites.push_back(createWorkflowContextTestSuite());
    suites.push_back(createLoggerTestSuite());
    suites.push_back(createCircuitBreakerTestSuite());
    suites.push_back(createCheckpointManagerTestSuite());

    return TestRunner::runAllSuites(suites);
}
