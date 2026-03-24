/**
 * @file test_framework.h
 * @brief 简单的单元测试框架
 *
 * 设计理念：
 * - 轻量级：无需外部依赖
 * - 易用：简单的断言宏
 * - 可扩展：支持自定义测试套件
 */

#ifndef WORKFLOW_SYSTEM_TEST_FRAMEWORK_H
#define WORKFLOW_SYSTEM_TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <cmath>
#include <memory>

namespace WorkflowSystem {
namespace Test {

// ========== 颜色输出支持 ==========
namespace Colors {
    const char* RESET = "\033[0m";
    const char* RED = "\033[31m";
    const char* GREEN = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* BLUE = "\033[34m";
    const char* CYAN = "\033[36m";
}

// ========== 测试结果统计 ==========
struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
    double durationMs;
};

struct TestSuiteResult {
    std::string suiteName;
    int totalTests;
    int passedTests;
    int failedTests;
    std::vector<TestResult> results;
};

// ========== 断言宏 ==========
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + message); \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(condition) \
    TEST_ASSERT((condition), "Expected true, got false")

#define TEST_ASSERT_FALSE(condition) \
    TEST_ASSERT(!(condition), "Expected false, got true")

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if (!((expected) == (actual))) { \
            throw std::runtime_error("Values not equal"); \
        } \
    } while(0)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    TEST_ASSERT((expected) != (actual), "Values should not be equal")

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT((ptr) == nullptr, "Expected nullptr")

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT((ptr) != nullptr,"Expected non-null pointer")

#define TEST_ASSERT_THROW(expression, exception_type) \
    do { \
        bool caught = false; \
        try { \
            (expression); \
        } catch (const exception_type&) { \
            caught = true; \
        } catch (...) { \
            caught = false; \
        } \
        if (!caught) { \
            throw std::runtime_error("Expected exception not thrown"); \
        } \
    } while(0)

#define TEST_ASSERT_NO_THROW(expression) \
    do { \
        try { \
            (void)(expression); \
        } catch (const std::exception& e) { \
            throw std::runtime_error(std::string("Unexpected exception: ") + e.what()); \
        } catch (...) { \
            throw std::runtime_error("Unexpected unknown exception"); \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQUAL(expected, actual, tolerance) \
    TEST_ASSERT(std::fabs((expected) - (actual)) < (tolerance),"Float values not equal within tolerance")

#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    TEST_ASSERT((expected) == (actual),std::string("String not equal: expected '") + (expected) + "', got '" + (actual) + "'")

// ========== 测试用例定义 ==========
class TestCase {
public:
    using TestFunc = std::function<void()>;

    TestCase(const std::string& name, TestFunc func)
        : name_(name), testFunc_(func) {}

    const std::string& getName() const { return name_; }

    void run() {
        auto startTime = std::chrono::steady_clock::now();
        try {
            testFunc_();
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(
                endTime - startTime).count();

            TestResult result;
            result.testName = name_;
            result.passed = true;
            result.message = "PASSED";
            result.durationMs = duration;
            currentResults_.push_back(result);
        } catch (const std::exception& e) {
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(
                endTime - startTime).count();

            TestResult result;
            result.testName = name_;
            result.passed = false;
            result.message = std::string("FAILED: ") + e.what();
            result.durationMs = duration;
            currentResults_.push_back(result);
        }
    }

    static std::vector<TestResult>& getCurrentResults() {
        return currentResults_;
    }

    static void clearResults() {
        currentResults_.clear();
    }

private:
    std::string name_;
    TestFunc testFunc_;
    static std::vector<TestResult> currentResults_;
};

// ========== 测试套件 ==========
class TestSuite {
public:
    TestSuite(const std::string& name) : suiteName_(name) {}

    void addTest(const std::string& testName,
               std::function<void()> testFunc) {
        tests_.push_back(TestCase(testName, testFunc));
    }

    TestSuiteResult run() const {
        TestSuiteResult result;
        result.suiteName = suiteName_;
        result.totalTests = tests_.size();
        result.passedTests = 0;
        result.failedTests = 0;

        std::cout << "\n" << Colors::CYAN << "========================================" << Colors::RESET << std::endl;
        std::cout << Colors::CYAN << "  运行测试套件: " << suiteName_ << Colors::RESET << std::endl;
        std::cout << Colors::CYAN << "========================================" << Colors::RESET << std::endl;

        for (auto& test : tests_) {
            test.run();
        }

        // 收集结果
        for (const auto& testResult : TestCase::getCurrentResults()) {
            result.results.push_back(testResult);
            if (testResult.passed) {
                result.passedTests++;
                std::cout << Colors::GREEN << "  [PASS] " << Colors::RESET
                          << testResult.testName
                          << " (" << testResult.durationMs << "ms)" << std::endl;
            } else {
                result.failedTests++;
                std::cout << Colors::RED << "  [FAIL] " << Colors::RESET
                          << testResult.testName
                          << " - " << testResult.message << std::endl;
            }
        }

        // 打印摘要
        std::cout << "\n" << Colors::CYAN << "----------------------------------------" << Colors::RESET << std::endl;
        std::cout << "总计: " << result.totalTests << " 测试" << std::endl;
        std::cout << Colors::GREEN << "通过: " << result.passedTests << Colors::RESET << std::endl;
        std::cout << Colors::RED << "失败: " << result.failedTests << Colors::RESET << std::endl;

        double successRate = (result.totalTests > 0) ?
            (static_cast<double>(result.passedTests) / result.totalTests * 100.0) : 0.0;

        if (result.failedTests == 0) {
            std::cout << Colors::GREEN << "成功率: 100%" << Colors::RESET << std::endl;
        } else {
            std::cout << "成功率: " << successRate << "%" << std::endl;
        }
        std::cout << Colors::CYAN << "----------------------------------------" << Colors::RESET << "\n" << std::endl;

        TestCase::clearResults();
        return result;
    }

private:
    std::string suiteName_;
    mutable std::vector<TestCase> tests_;
};

// ========== 测试运行器 ==========
class TestRunner {
public:
    static int runAllSuites(const std::vector<TestSuite>& suites) {
        int totalSuites = 0;
        int passedSuites = 0;
        int totalTests = 0;
        int passedTests = 0;

        std::cout << "\n" << Colors::BLUE << "╔════════════════════════════════════════╗" << Colors::RESET << std::endl;
        std::cout << Colors::BLUE << "║        WorkflowSystem 单元测试运行器        ║" << Colors::RESET << std::endl;
        std::cout << Colors::BLUE << "╚════════════════════════════════════════╝" << Colors::RESET << std::endl;

        for (const auto& suite : suites) {
            totalSuites++;
            auto result = suite.run();

            totalTests += result.totalTests;
            passedTests += result.passedTests;

            if (result.failedTests == 0) {
                passedSuites++;
            }
        }

        // 总体摘要
        std::cout << "\n" << Colors::BLUE << "========================================" << Colors::RESET << std::endl;
        std::cout << Colors::BLUE << "          总体测试摘要" << Colors::RESET << std::endl;
        std::cout << Colors::BLUE << "========================================" << Colors::RESET << std::endl;
        std::cout << "测试套件: " << passedSuites << "/" << totalSuites << " 通过" << std::endl;
        std::cout << "测试用例: " << passedTests << "/" << totalTests << " 通过" << std::endl;

        double overallSuccessRate = (totalTests > 0) ?
            (static_cast<double>(passedTests) / totalTests * 100.0) : 0.0;

        std::cout << "总体成功率: " << overallSuccessRate << "%" << std::endl;

        if (passedTests == totalTests) {
            std::cout << Colors::GREEN << "\n🎉 所有测试通过！" << Colors::RESET << "\n" << std::endl;
            return 0;
        } else {
            std::cout << Colors::RED << "\n❌ 存在失败的测试" << Colors::RESET << "\n" << std::endl;
            return 1;
        }
    }
};

// 静态成员定义
std::vector<TestResult> TestCase::currentResults_;

} // namespace Test
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_TEST_FRAMEWORK_H
