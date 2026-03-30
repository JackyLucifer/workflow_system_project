/**
 * @file test_service_locator.cpp
 * @brief ServiceLocator单元测试
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <string>
#include "workflow_system/plugin/core/ServiceLocator.hpp"

using namespace WorkflowSystem::Plugin;

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "运行测试: " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "[通过]" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "[失败] " << e.what() << std::endl; \
        tests_failed++; \
    } catch (...) { \
        std::cout << "[失败] 未知异常" << std::endl; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error("断言失败: " #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("断言失败: " #a " != " #b)
#define ASSERT_NE(a, b) if ((a) == (b)) throw std::runtime_error("断言失败: " #a " == " #b)

// 测试服务接口
class ILogger {
public:
    virtual ~ILogger() {}
    virtual void log(const std::string& msg) = 0;
    virtual std::string lastMessage() const = 0;
};

class MockLogger : public ILogger {
public:
    void log(const std::string& msg) override { lastMsg_ = msg; }
    std::string lastMessage() const override { return lastMsg_; }
private:
    std::string lastMsg_;
};

class IDatabase {
public:
    virtual ~IDatabase() {}
    virtual std::string query(const std::string& sql) = 0;
};

class MockDatabase : public IDatabase {
public:
    std::string query(const std::string& sql) override { return "result: " + sql; }
};

// 测试基本注册和获取
TEST(basic_register_get) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    MockLogger logger;
    locator.registerTyped<ILogger>("logger", &logger);
    
    ASSERT_TRUE(locator.hasService("logger"));
    
    ILogger* retrieved = locator.getTyped<ILogger>("logger");
    ASSERT_TRUE(retrieved != nullptr);
    ASSERT_TRUE(retrieved == &logger);
    
    retrieved->log("测试消息");
    ASSERT_EQ(logger.lastMessage(), "测试消息");
    
    locator.clear();
}

// 测试多个服务
TEST(multiple_services) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    MockLogger logger;
    MockDatabase db;
    
    locator.registerTyped<ILogger>("logger", &logger);
    locator.registerTyped<IDatabase>("database", &db);
    
    auto names = locator.getServiceNames();
    ASSERT_TRUE(names.size() >= 2);
    
    ILogger* log = locator.getTyped<ILogger>("logger");
    IDatabase* database = locator.getTyped<IDatabase>("database");
    
    ASSERT_TRUE(log != nullptr);
    ASSERT_TRUE(database != nullptr);
    ASSERT_EQ(database->query("SELECT 1"), "result: SELECT 1");
    
    locator.clear();
}

// 测试注销服务
TEST(unregister_service) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    MockLogger logger;
    locator.registerTyped<ILogger>("logger", &logger);
    ASSERT_TRUE(locator.hasService("logger"));
    
    locator.unregisterService("logger");
    ASSERT_TRUE(!locator.hasService("logger"));
    
    ILogger* retrieved = locator.getTyped<ILogger>("logger");
    ASSERT_TRUE(retrieved == nullptr);
    
    locator.clear();
}

// 测试shared_ptr服务
TEST(shared_ptr_service) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    auto logger = std::make_shared<MockLogger>();
    locator.registerTyped<ILogger>("logger", logger.get());
    
    ILogger* retrieved = locator.getTyped<ILogger>("logger");
    ASSERT_TRUE(retrieved != nullptr);
    retrieved->log("shared测试");
    ASSERT_EQ(logger->lastMessage(), "shared测试");
    
    locator.clear();
}

// 测试获取不存在的服务
TEST(get_nonexistent_service) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    ILogger* log = locator.getTyped<ILogger>("nonexistent");
    ASSERT_TRUE(log == nullptr);
    
    ASSERT_TRUE(!locator.hasService("nonexistent"));
}

// 测试服务覆盖
TEST(service_override) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    MockLogger logger1;
    MockLogger logger2;
    
    locator.registerTyped<ILogger>("logger", &logger1);
    locator.registerTyped<ILogger>("logger", &logger2);
    
    ILogger* retrieved = locator.getTyped<ILogger>("logger");
    ASSERT_TRUE(retrieved == &logger2);
    
    locator.clear();
}

// 测试空服务名
TEST(empty_service_name) {
    ServiceLocator& locator = ServiceLocator::instance();
    locator.clear();
    
    MockLogger logger;
    locator.registerTyped<ILogger>("", &logger);
    
    ILogger* retrieved = locator.getTyped<ILogger>("");
    ASSERT_TRUE(retrieved == &logger);
    
    locator.clear();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ServiceLocator 单元测试" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    RUN_TEST(basic_register_get);
    RUN_TEST(multiple_services);
    RUN_TEST(unregister_service);
    RUN_TEST(shared_ptr_service);
    RUN_TEST(get_nonexistent_service);
    RUN_TEST(service_override);
    RUN_TEST(empty_service_name);
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  测试结果: 通过 " << tests_passed << "/" << (tests_passed + tests_failed) << std::endl;
    std::cout << "========================================" << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
