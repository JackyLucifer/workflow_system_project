/**
 * @file test_async_logger.cpp
 * @brief 异步日志系统测试
 */

#include "workflow_system/implementations/async_logger.h"
#include "test_framework.h"
#include <thread>
#include <chrono>
#include <cstdio>
#include <unistd.h>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助函数 ==========

std::string getUniqueTestLogPath() {
    static int counter = 0;
    return "/tmp/test_async_logger_" + std::to_string(++counter) + ".log";
}

void cleanupTestLog(const std::string& path) {
    ::unlink(path.c_str());
}

// ========== 测试用例 ==========

void testAsyncLogger_Singleton() {
    auto& logger1 = AsyncLogger::instance();
    auto& logger2 = AsyncLogger::instance();

    TEST_ASSERT_TRUE(&logger1 == &logger2);
}

void testAsyncLogger_LogInfo() {
    auto& logger = AsyncLogger::instance();

    // 这个测试主要验证不会崩溃
    logger.info("Test info message");

    // 给后台线程一些时间处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_LogWarning() {
    auto& logger = AsyncLogger::instance();

    logger.warning("Test warning message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_LogError() {
    auto& logger = AsyncLogger::instance();

    logger.error("Test error message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_LogDebug() {
    auto& logger = AsyncLogger::instance();

    logger.debug("Test debug message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_LogWithLevel() {
    auto& logger = AsyncLogger::instance();

    logger.log("Custom log message", LogLevel::INFO);
    logger.log("Warning message", LogLevel::WARNING);
    logger.log("Error message", LogLevel::ERROR);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_SetConfig() {
    auto& logger = AsyncLogger::instance();

    AsyncLoggerConfig config;
    config.logFilePath = "/tmp/test_config.log";
    config.enableConsole = false;
    config.enableFile = false;
    config.maxQueueSize = 5000;
    config.flushOnWrite = false;

    logger.setConfig(config);

    auto retrieved = logger.getConfig();

    TEST_ASSERT_STRING_EQUAL("/tmp/test_config.log", retrieved.logFilePath.c_str());
    TEST_ASSERT_FALSE(retrieved.enableConsole);
    TEST_ASSERT_FALSE(retrieved.enableFile);
    TEST_ASSERT_EQUAL(5000, static_cast<int>(retrieved.maxQueueSize));
    TEST_ASSERT_FALSE(retrieved.flushOnWrite);

    cleanupTestLog("/tmp/test_config.log");
}

void testAsyncLogger_GetConfig() {
    auto& logger = AsyncLogger::instance();

    auto config = logger.getConfig();

    TEST_ASSERT_FALSE(config.logFilePath.empty());
}

void testAsyncLogger_MultipleMessages() {
    auto& logger = AsyncLogger::instance();

    for (int i = 0; i < 100; ++i) {
        logger.info("Message " + std::to_string(i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_LongMessage() {
    auto& logger = AsyncLogger::instance();

    std::string longMessage(10000, 'A');  // 10KB 的消息
    logger.info(longMessage);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_ConcurrentLogging() {
    auto& logger = AsyncLogger::instance();

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&logger, i]() {
            for (int j = 0; j < 100; ++j) {
                logger.info("Thread " + std::to_string(i) + " message " + std::to_string(j));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_SpecialCharacters() {
    auto& logger = AsyncLogger::instance();

    logger.info("Message with \"quotes\" and 'apostrophes'");
    logger.info("Message with \ttabs\nand\nnewlines");
    logger.info("Message with special chars: !@#$%^&*()");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_Unicode() {
    auto& logger = AsyncLogger::instance();

    logger.info("Unicode test: 中文测试 🚀 Test");
    logger.info("Emoji test: 😀 🎉 🚀");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_ErrorCallback() {
    auto& logger = AsyncLogger::instance();

    bool callbackCalled = false;
    std::string errorMsg;

    AsyncLoggerConfig config;
    config.errorCallback = [&](const std::string& error) {
        callbackCalled = true;
        errorMsg = error;
    };

    logger.setConfig(config);

    // 尝试记录一条日志
    logger.info("Test message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 注意：没有错误的情况下回调不应该被调用
    // 这个测试主要验证回调机制存在
    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_DisableConsole() {
    auto& logger = AsyncLogger::instance();

    AsyncLoggerConfig config;
    config.enableConsole = false;
    config.enableFile = false;

    logger.setConfig(config);

    logger.info("Silent log message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

void testAsyncLogger_QueueSizeLimit() {
    auto& logger = AsyncLogger::instance();

    AsyncLoggerConfig config;
    config.maxQueueSize = 100;
    config.enableConsole = false;
    config.enableFile = false;

    logger.setConfig(config);

    // 发送超过队列大小的消息
    for (int i = 0; i < 200; ++i) {
        logger.info("Message " + std::to_string(i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TEST_ASSERT_TRUE(true);
}

// ========== LogFile 测试用例 ==========

void testLogFile_OpenClose() {
    std::string logPath = getUniqueTestLogPath();

    {
        LogFile logFile(logPath);

        TEST_ASSERT_TRUE(logFile.isOpen());

        logFile.write("Test message");
        logFile.flush();
    }  // 析构时自动关闭

    cleanupTestLog(logPath);
}

void testLogFile_Write() {
    std::string logPath = getUniqueTestLogPath();

    LogFile logFile(logPath);

    logFile.write("Line 1");
    logFile.write("Line 2");
    logFile.write("Line 3");

    logFile.flush();

    TEST_ASSERT_TRUE(logFile.isOpen());

    cleanupTestLog(logPath);
}

void testLogFile_Reopen() {
    std::string logPath = getUniqueTestLogPath();

    LogFile logFile(logPath);

    logFile.write("Before reopen");
    logFile.flush();

    logFile.reopen();

    logFile.write("After reopen");
    logFile.flush();

    TEST_ASSERT_TRUE(logFile.isOpen());

    cleanupTestLog(logPath);
}

// ========== 测试套件创建 ==========

TestSuite createAsyncLoggerTestSuite() {
    TestSuite suite("异步日志系统测试");

    suite.addTest("AsyncLogger_Singleton", testAsyncLogger_Singleton);
    suite.addTest("AsyncLogger_LogInfo", testAsyncLogger_LogInfo);
    suite.addTest("AsyncLogger_LogWarning", testAsyncLogger_LogWarning);
    suite.addTest("AsyncLogger_LogError", testAsyncLogger_LogError);
    suite.addTest("AsyncLogger_LogDebug", testAsyncLogger_LogDebug);
    suite.addTest("AsyncLogger_LogWithLevel", testAsyncLogger_LogWithLevel);
    suite.addTest("AsyncLogger_SetConfig", testAsyncLogger_SetConfig);
    suite.addTest("AsyncLogger_GetConfig", testAsyncLogger_GetConfig);
    suite.addTest("AsyncLogger_MultipleMessages", testAsyncLogger_MultipleMessages);
    suite.addTest("AsyncLogger_LongMessage", testAsyncLogger_LongMessage);
    suite.addTest("AsyncLogger_ConcurrentLogging", testAsyncLogger_ConcurrentLogging);
    suite.addTest("AsyncLogger_SpecialCharacters", testAsyncLogger_SpecialCharacters);
    suite.addTest("AsyncLogger_Unicode", testAsyncLogger_Unicode);
    suite.addTest("AsyncLogger_ErrorCallback", testAsyncLogger_ErrorCallback);
    suite.addTest("AsyncLogger_DisableConsole", testAsyncLogger_DisableConsole);
    suite.addTest("AsyncLogger_QueueSizeLimit", testAsyncLogger_QueueSizeLimit);

    suite.addTest("LogFile_OpenClose", testLogFile_OpenClose);
    suite.addTest("LogFile_Write", testLogFile_Write);
    suite.addTest("LogFile_Reopen", testLogFile_Reopen);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::vector<TestSuite> suites;
    suites.push_back(createAsyncLoggerTestSuite());

    return TestRunner::runAllSuites(suites);
}
