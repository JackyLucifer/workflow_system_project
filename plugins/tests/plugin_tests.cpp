/**
 * @file plugin_tests.cpp
 * @brief 插件系统完整单元测试
 */

#include "plugin_interface.h"
#include "plugin_loader.h"
#include "service_registry.h"
#include "dependency_resolver.h"
#include "message_bus.h"
#include "plugin_config.h"
#include "resource_manager.h"
#include "version_manager.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <thread>
#include <chrono>

using namespace WorkflowSystem::Plugin;

// 测试计数器
int totalTests = 0;
int passedTests = 0;
int failedTests = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        totalTests++; \
        if (condition) { \
            passedTests++; \
            std::cout << "  ✓ " << message << std::endl; \
        } else { \
            failedTests++; \
            std::cout << "  ✗ " << message << " (FAILED)" << std::endl; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_TRUE(condition, message) \
    TEST_ASSERT(condition, message)

#define TEST_ASSERT_FALSE(condition, message) \
    TEST_ASSERT(!(condition), message)

// ========== SemanticVersion 测试 ==========

void test_version_parsing() {
    std::cout << "\n[SemanticVersion 解析测试]" << std::endl;
    
    SemanticVersion v1 = SemanticVersion::parse("1.2.3");
    TEST_ASSERT_EQUAL(1, v1.major, "解析主版本号");
    TEST_ASSERT_EQUAL(2, v1.minor, "解析次版本号");
    TEST_ASSERT_EQUAL(3, v1.patch, "解析补丁版本号");
    
    SemanticVersion v2 = SemanticVersion::parse("2.0.0-alpha");
    TEST_ASSERT_EQUAL(2, v2.major, "解析带预发布版本");
    TEST_ASSERT_EQUAL(0, v2.minor, "预发布版本次版本号");
    TEST_ASSERT_EQUAL("alpha", v2.prerelease, "预发布标签");
    
    SemanticVersion v3 = SemanticVersion::parse("1.0.0+build.123");
    TEST_ASSERT_EQUAL("build.123", v3.build, "解析构建元数据");
}

void test_version_comparison() {
    std::cout << "\n[SemanticVersion 比较测试]" << std::endl;
    
    SemanticVersion v1(1, 0, 0);
    SemanticVersion v2(1, 0, 1);
    SemanticVersion v3(1, 1, 0);
    SemanticVersion v4(2, 0, 0);
    SemanticVersion v5(1, 0, 0);
    
    TEST_ASSERT_TRUE(v1 < v2, "1.0.0 < 1.0.1");
    TEST_ASSERT_TRUE(v2 < v3, "1.0.1 < 1.1.0");
    TEST_ASSERT_TRUE(v3 < v4, "1.1.0 < 2.0.0");
    TEST_ASSERT_TRUE(v1 == v5, "1.0.0 == 1.0.0");
    TEST_ASSERT_TRUE(v1 <= v5, "1.0.0 <= 1.0.0");
    TEST_ASSERT_TRUE(v1 >= v5, "1.0.0 >= 1.0.0");
    TEST_ASSERT_TRUE(v1 != v2, "1.0.0 != 1.0.1");
}

void test_version_to_string() {
    std::cout << "\n[SemanticVersion 字符串化测试]" << std::endl;
    
    SemanticVersion v1(1, 2, 3);
    TEST_ASSERT_EQUAL(std::string("1.2.3"), v1.toString(), "基本版本字符串化");
    
    SemanticVersion v2(2, 0, 0, "beta", "123");
    std::string expected = "2.0.0-beta+123";
    TEST_ASSERT_EQUAL(expected, v2.toString(), "完整版本字符串化");
}

// ========== VersionConstraint 测试 ==========

void test_exact_constraint() {
    std::cout << "\n[精确版本约束测试]" << std::endl;
    
    VersionConstraint c = VersionConstraint::parse("=1.0.0");
    TEST_ASSERT_TRUE(c.isSatisfiedBy(SemanticVersion(1, 0, 0)), "=1.0.0 满足 1.0.0");
    TEST_ASSERT_FALSE(c.isSatisfiedBy(SemanticVersion(1, 0, 1)), "=1.0.0 不满足 1.0.1");
    TEST_ASSERT_FALSE(c.isSatisfiedBy(SemanticVersion(0, 9, 9)), "=1.0.0 不满足 0.9.9");
}

void test_range_constraints() {
    std::cout << "\n[范围版本约束测试]" << std::endl;
    
    VersionConstraint gt = VersionConstraint::parse(">1.0.0");
    TEST_ASSERT_FALSE(gt.isSatisfiedBy(SemanticVersion(1, 0, 0)), ">1.0.0 不满足 1.0.0");
    TEST_ASSERT_TRUE(gt.isSatisfiedBy(SemanticVersion(1, 0, 1)), ">1.0.0 满足 1.0.1");
    
    VersionConstraint gte = VersionConstraint::parse(">=1.0.0");
    TEST_ASSERT_TRUE(gte.isSatisfiedBy(SemanticVersion(1, 0, 0)), ">=1.0.0 满足 1.0.0");
    TEST_ASSERT_TRUE(gte.isSatisfiedBy(SemanticVersion(2, 0, 0)), ">=1.0.0 满足 2.0.0");
    TEST_ASSERT_FALSE(gte.isSatisfiedBy(SemanticVersion(0, 9, 9)), ">=1.0.0 不满足 0.9.9");
    
    VersionConstraint lt = VersionConstraint::parse("<2.0.0");
    TEST_ASSERT_TRUE(lt.isSatisfiedBy(SemanticVersion(1, 9, 9)), "<2.0.0 满足 1.9.9");
    TEST_ASSERT_FALSE(lt.isSatisfiedBy(SemanticVersion(2, 0, 0)), "<2.0.0 不满足 2.0.0");
}

void test_caret_constraint() {
    std::cout << "\n[Caret 约束测试 (^)" << std::endl;
    
    VersionConstraint caret = VersionConstraint::parse("^1.2.3");
    TEST_ASSERT_TRUE(caret.isSatisfiedBy(SemanticVersion(1, 2, 3)), "^1.2.3 满足 1.2.3");
    TEST_ASSERT_TRUE(caret.isSatisfiedBy(SemanticVersion(1, 2, 4)), "^1.2.3 满足 1.2.4");
    TEST_ASSERT_TRUE(caret.isSatisfiedBy(SemanticVersion(1, 3, 0)), "^1.2.3 满足 1.3.0");
    TEST_ASSERT_TRUE(caret.isSatisfiedBy(SemanticVersion(1, 9, 9)), "^1.2.3 满足 1.9.9");
    TEST_ASSERT_FALSE(caret.isSatisfiedBy(SemanticVersion(2, 0, 0)), "^1.2.3 不满足 2.0.0");
    TEST_ASSERT_FALSE(caret.isSatisfiedBy(SemanticVersion(1, 2, 2)), "^1.2.3 不满足 1.2.2");
    
    VersionConstraint caret0 = VersionConstraint::parse("^0.2.3");
    TEST_ASSERT_TRUE(caret0.isSatisfiedBy(SemanticVersion(0, 2, 3)), "^0.2.3 满足 0.2.3");
    TEST_ASSERT_TRUE(caret0.isSatisfiedBy(SemanticVersion(0, 2, 9)), "^0.2.3 满足 0.2.9");
    TEST_ASSERT_FALSE(caret0.isSatisfiedBy(SemanticVersion(0, 3, 0)), "^0.2.3 不满足 0.3.0");
}

void test_tilde_constraint() {
    std::cout << "\n[Tilde 约束测试 (~)]" << std::endl;
    
    VersionConstraint tilde = VersionConstraint::parse("~1.2.3");
    TEST_ASSERT_TRUE(tilde.isSatisfiedBy(SemanticVersion(1, 2, 3)), "~1.2.3 满足 1.2.3");
    TEST_ASSERT_TRUE(tilde.isSatisfiedBy(SemanticVersion(1, 2, 9)), "~1.2.3 满足 1.2.9");
    TEST_ASSERT_FALSE(tilde.isSatisfiedBy(SemanticVersion(1, 3, 0)), "~1.2.3 不满足 1.3.0");
}

// ========== DependencyResolver 测试 ==========

void test_dependency_resolver_basic() {
    std::cout << "\n[依赖解析器基本测试]" << std::endl;
    
    DependencyResolver resolver;
    resolver.registerNode("A", {"B", "C"});
    resolver.registerNode("B", {});
    resolver.registerNode("C", {"B"});
    
    DependencyResult result = resolver.resolve();
    TEST_ASSERT_TRUE(result.success, "解析成功");
    TEST_ASSERT_EQUAL(3, static_cast<int>(result.loadOrder.size()), "3个节点");
    
    // B 必须在 A 和 C 之前
    auto itB = std::find(result.loadOrder.begin(), result.loadOrder.end(), "B");
    auto itA = std::find(result.loadOrder.begin(), result.loadOrder.end(), "A");
    auto itC = std::find(result.loadOrder.begin(), result.loadOrder.end(), "C");
    
    TEST_ASSERT_TRUE(itB < itA, "B 在 A 之前");
    TEST_ASSERT_TRUE(itB < itC, "B 在 C 之前");
}

void test_dependency_circular_detection() {
    std::cout << "\n[循环依赖检测测试]" << std::endl;
    
    DependencyResolver resolver;
    resolver.registerNode("A", {"B"});
    resolver.registerNode("B", {"C"});
    resolver.registerNode("C", {"A"});  // 循环
    
    DependencyResult result = resolver.resolve();
    TEST_ASSERT_FALSE(result.success, "检测到循环依赖");
    TEST_ASSERT_TRUE(result.circularDependencies.size() > 0, "记录循环依赖");
}

void test_missing_dependency() {
    std::cout << "\n[缺失依赖检测测试]" << std::endl;
    
    DependencyResolver resolver;
    resolver.registerNode("A", {"B"});  // B 不存在
    
    DependencyResult result = resolver.resolve();
    TEST_ASSERT_FALSE(result.success, "检测到缺失依赖");
    TEST_ASSERT_TRUE(result.missingDependencies.size() > 0, "记录缺失依赖");
}

void test_priority_ordering() {
    std::cout << "\n[优先级排序测试]" << std::endl;
    
    DependencyResolver resolver;
    resolver.registerNode("low", {}, 1);
    resolver.registerNode("high", {}, 10);
    resolver.registerNode("medium", {}, 5);
    
    DependencyResult result = resolver.resolve();
    TEST_ASSERT_TRUE(result.success, "解析成功");
    
    if (result.loadOrder.size() >= 3) {
        TEST_ASSERT_EQUAL(std::string("high"), result.loadOrder[0], "高优先级在前");
    }
}

// ========== MessageBus 测试 ==========

void test_message_bus_subscribe_publish() {
    std::cout << "\n[消息总线订阅发布测试]" << std::endl;
    
    MessageBus& bus = MessageBus::getInstance();
    bus.clear();
    
    int receiveCount = 0;
    uint64_t subId = bus.subscribe("test.topic", [&](const Message& msg) {
        receiveCount++;
    });
    
    bus.publish("test.topic", "data1");
    bus.publish("test.topic", "data2");
    
    TEST_ASSERT_EQUAL(2, receiveCount, "收到2条消息");
    
    bus.unsubscribe(subId);
    bus.publish("test.topic", "data3");
    TEST_ASSERT_EQUAL(2, receiveCount, "取消订阅后不再收到");
}

void test_message_bus_multiple_subscribers() {
    std::cout << "\n[消息总线多订阅者测试]" << std::endl;
    
    MessageBus& bus = MessageBus::getInstance();
    bus.clear();
    
    int count1 = 0, count2 = 0;
    
    bus.subscribe("multi.topic", [&](const Message& msg) { count1++; });
    bus.subscribe("multi.topic", [&](const Message& msg) { count2++; });
    
    bus.publish("multi.topic", "test");
    
    TEST_ASSERT_EQUAL(1, count1, "订阅者1收到");
    TEST_ASSERT_EQUAL(1, count2, "订阅者2收到");
}

void test_message_bus_filter() {
    std::cout << "\n[消息总线过滤器测试]" << std::endl;
    
    MessageBus& bus = MessageBus::getInstance();
    bus.clear();
    
    int count = 0;
    
    bus.subscribeWithFilter("filter.topic",
        [&](const Message& msg) { count++; },
        [](const Message& msg) { return msg.data == "pass"; }
    );
    
    bus.publish("filter.topic", "pass");
    bus.publish("filter.topic", "block");
    
    TEST_ASSERT_EQUAL(1, count, "过滤器只接收匹配消息");
}

// ========== PluginConfigManager 测试 ==========

void test_config_basic() {
    std::cout << "\n[配置管理基本测试]" << std::endl;
    
    PluginConfigManager& config = PluginConfigManager::getInstance();
    config.clear();
    
    config.setGlobal("timeout", ConfigValue::fromInt(5000));
    TEST_ASSERT_EQUAL(5000, config.getGlobalInt("timeout", 0), "设置全局配置");
    
    config.setPluginConfig("test_plugin", "enabled", ConfigValue::fromBool(true));
    TEST_ASSERT_TRUE(config.getPluginBool("test_plugin", "enabled", false), "设置插件配置");
}

bool compareDouble(double a, double b, double epsilon) {
    return (a - epsilon < b) && (b < a + epsilon);
}

void test_config_types() {
    std::cout << "\n[配置类型测试]" << std::endl;
    
    PluginConfigManager& config = PluginConfigManager::getInstance();
    config.clear();
    
    config.setGlobal("str_key", ConfigValue::fromString("hello"));
    config.setGlobal("int_key", ConfigValue::fromInt(42));
    config.setGlobal("double_key", ConfigValue::fromDouble(3.14));
    config.setGlobal("bool_key", ConfigValue::fromBool(true));
    
    TEST_ASSERT_EQUAL(std::string("hello"), config.getGlobalString("str_key", ""), "字符串类型");
    TEST_ASSERT_EQUAL(42, config.getGlobalInt("int_key", 0), "整数类型");
    TEST_ASSERT_TRUE(compareDouble(3.14, config.getGlobal("double_key").asDouble(0.0), 0.001), "浮点类型");
    TEST_ASSERT_TRUE(config.getGlobalBool("bool_key", false), "布尔类型");
}

void test_config_json() {
    std::cout << "\n[配置JSON测试]" << std::endl;
    
    PluginConfigManager& config = PluginConfigManager::getInstance();
    config.clear();
    
    config.setGlobal("version", ConfigValue::fromString("1.0"));
    
    std::string json = config.toJson();
    TEST_ASSERT_TRUE(json.find("version") != std::string::npos, "导出JSON");
    TEST_ASSERT_TRUE(json.find("1.0") != std::string::npos, "JSON包含值");
    
    PluginConfigManager& config2 = PluginConfigManager::getInstance();
    config2.clear();
    config2.fromJson(json);
    TEST_ASSERT_EQUAL(std::string("1.0"), config2.getGlobalString("version", ""), "从JSON导入");
}

// ========== ResourceManager 测试 ==========

void test_resource_registration() {
    std::cout << "\n[资源注册测试]" << std::endl;
    
    ResourceManager& rm = ResourceManager::getInstance();
    
    int* ptr = new int(42);
    uint64_t id = rm.registerMemory(ptr, sizeof(int), "test_plugin");
    
    TEST_ASSERT_TRUE(id > 0, "注册内存资源");
    TEST_ASSERT_EQUAL(1, static_cast<int>(rm.getResourceCount()), "资源计数");
    
    rm.releaseResource(id);
    TEST_ASSERT_EQUAL(0, static_cast<int>(rm.getResourceCount()), "释放后计数");
}

void test_resource_owner_cleanup() {
    std::cout << "\n[资源所有者清理测试]" << std::endl;
    
    ResourceManager& rm = ResourceManager::getInstance();
    
    char* buf1 = new char[100];
    char* buf2 = new char[200];
    
    rm.registerMemory(buf1, 100, "owner_test");
    rm.registerMemory(buf2, 200, "owner_test");
    
    TEST_ASSERT_EQUAL(2, static_cast<int>(rm.getOwnerResourceCount("owner_test")), "注册2个资源");
    
    int released = rm.releaseOwnerResources("owner_test");
    TEST_ASSERT_EQUAL(2, released, "清理2个资源");
    TEST_ASSERT_EQUAL(0, static_cast<int>(rm.getOwnerResourceCount("owner_test")), "清理后为0");
}

void test_resource_guard() {
    std::cout << "\n[资源守卫测试]" << std::endl;
    
    size_t countBefore = ResourceManager::getInstance().getResourceCount();
    
    {
        ResourceGuard guard("guard_test");
        int* ptr = new int(100);
        guard.addMemory(ptr, sizeof(int));
        
        TEST_ASSERT_EQUAL(1, static_cast<int>(guard.count()), "守卫管理1个资源");
    }
    
    size_t countAfter = ResourceManager::getInstance().getResourceCount();
    TEST_ASSERT_EQUAL(countBefore, countAfter, "守卫析构后资源释放");
}

// ========== ServiceRegistry 测试 ==========

void test_service_registration() {
    std::cout << "\n[服务注册测试]" << std::endl;
    
    ServiceRegistry& sr = ServiceRegistry::getInstance();
    sr.clear();
    
    int service = 42;
    TEST_ASSERT_TRUE(sr.registerInstance("test_service", &service), "注册服务");
    TEST_ASSERT_TRUE(sr.has("test_service"), "服务存在");
    
    int* retrieved = sr.get<int>("test_service");
    TEST_ASSERT_TRUE(retrieved != nullptr, "获取服务");
    TEST_ASSERT_EQUAL(42, *retrieved, "服务值正确");
}

void test_shared_service() {
    std::cout << "\n[共享服务测试]" << std::endl;
    
    ServiceRegistry& sr = ServiceRegistry::getInstance();
    sr.clear();
    
    auto sharedService = std::make_shared<std::string>("shared");
    TEST_ASSERT_TRUE(sr.registerShared("shared_service", sharedService), "注册共享服务");
    
    void* raw = sr.getRaw("shared_service");
    TEST_ASSERT_TRUE(raw != nullptr, "获取原始指针");
}

// ========== 运行所有测试 ==========

void run_all_tests() {
    std::cout << "\n╔════════════════════════════════════════╗\n"
              << "║     插件系统单元测试                     ║\n"
              << "╚════════════════════════════════════════╝" << std::endl;
    
    // SemanticVersion 测试
    test_version_parsing();
    test_version_comparison();
    test_version_to_string();
    
    // VersionConstraint 测试
    test_exact_constraint();
    test_range_constraints();
    test_caret_constraint();
    test_tilde_constraint();
    
    // DependencyResolver 测试
    test_dependency_resolver_basic();
    test_dependency_circular_detection();
    test_missing_dependency();
    test_priority_ordering();
    
    // MessageBus 测试
    test_message_bus_subscribe_publish();
    test_message_bus_multiple_subscribers();
    test_message_bus_filter();
    
    // PluginConfigManager 测试
    test_config_basic();
    test_config_types();
    test_config_json();
    
    // ResourceManager 测试
    test_resource_registration();
    test_resource_owner_cleanup();
    test_resource_guard();
    
    // ServiceRegistry 测试
    test_service_registration();
    test_shared_service();
    
    // 输出统计
    std::cout << "\n╔════════════════════════════════════════╗\n"
              << "║              测试结果                   ║\n"
              << "╠════════════════════════════════════════╣\n"
              << "║  总计: " << totalTests << " 测试                      ║\n"
              << "║  通过: " << passedTests << "                          ║\n"
              << "║  失败: " << failedTests << "                          ║\n"
              << "╚════════════════════════════════════════╝" << std::endl;
    
    if (failedTests == 0) {
        std::cout << "\n✅ 所有测试通过！" << std::endl;
    } else {
        std::cout << "\n❌ 有 " << failedTests << " 个测试失败" << std::endl;
    }
}

int main() {
    run_all_tests();
    return (failedTests == 0) ? 0 : 1;
}
