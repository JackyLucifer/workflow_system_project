/**
 * @file test_core_types.cpp
 * @brief 核心类型测试用例
 */

#include "core/types.h"
#include "core/any.h"
#include "core/utils.h"
#include "core/logger.h"
#include "test_framework.h"

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

// ========== IdGenerator 测试 ==========
void testIdGenerator_GenerateUnique() {
    std::string id1 = IdGenerator::generate();
    std::string id2 = IdGenerator::generate();

    TEST_ASSERT_FALSE(id1.empty());
    TEST_ASSERT_FALSE(id2.empty());
    TEST_ASSERT_TRUE(id1 != id2);
}

void testIdGenerator_Length() {
    std::string id = IdGenerator::generate();
    TEST_ASSERT_TRUE(id.length() > 0);
}

// ========== TimeUtils 测试 ==========
void testTimeUtils_GetCurrentMs() {
    int64_t t1 = TimeUtils::getCurrentMs();
    TEST_ASSERT_TRUE(t1 > 0);
}

void testTimeUtils_GetCurrentTimestamp() {
    std::string ts = TimeUtils::getCurrentTimestamp();
    TEST_ASSERT_FALSE(ts.empty());
    TEST_ASSERT_TRUE(ts.length() > 0);
}

void testTimeUtils_FormatDuration() {
    int64_t duration = 1234567;
    std::string formatted = TimeUtils::formatDuration(duration);
    TEST_ASSERT_FALSE(formatted.empty());
}

// ========== WorkflowState 测试 ==========
void testWorkflowState_EnumValues() {
    TEST_ASSERT_EQUAL(0, static_cast<int>(WorkflowState::IDLE));
    TEST_ASSERT_EQUAL(1, static_cast<int>(WorkflowState::INITIALIZING));
}

void testWorkflowState_StringConversion() {
    std::string stateStr = workflowStateToString(WorkflowState::RUNNING);
    TEST_ASSERT_STRING_EQUAL("running", stateStr);
}

// ========== Test Suite 定义 ==========
TestSuite createCoreTypesTestSuite() {
    TestSuite suite("核心类型测试");

    // Any 类型测试
    suite.addTest("Any_Empty", testAny_Empty);
    suite.addTest("Any_IntValue", testAny_IntValue);
    suite.addTest("Any_StringValue", testAny_StringValue);
    suite.addTest("Any_DoubleValue", testAny_DoubleValue);
    suite.addTest("Any_Copy", testAny_Copy);
    suite.addTest("Any_Assignment", testAny_Assignment);

    // IdGenerator 测试
    suite.addTest("IdGenerator_GenerateUnique", testIdGenerator_GenerateUnique);
    suite.addTest("IdGenerator_Length", testIdGenerator_Length);

    // TimeUtils 测试
    suite.addTest("TimeUtils_GetCurrentMs", testTimeUtils_GetCurrentMs);
    suite.addTest("TimeUtils_GetCurrentTimestamp", testTimeUtils_GetCurrentTimestamp);
    suite.addTest("TimeUtils_FormatDuration", testTimeUtils_FormatDuration);

    // WorkflowState 测试
    suite.addTest("WorkflowState_EnumValues", testWorkflowState_EnumValues);
    suite.addTest("WorkflowState_StringConversion", testWorkflowState_StringConversion);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // 禁用日志以获得更清晰的测试输出
    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createCoreTypesTestSuite());

    return TestRunner::runAllSuites(suites);
}
