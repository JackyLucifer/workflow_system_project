/**
 * @file test_channel.cpp
 * @brief Channel单元测试
 */

#include <iostream>
#include <cassert>
#include <atomic>
#include <chrono>
#include <thread>
#include "workflow_system/plugin/communication/Channel.hpp"

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

// 测试DataPacket基本功能
TEST(data_packet_basic) {
    DataPacket packet;
    packet.topic = "test.topic";
    packet.source = "test_plugin";
    packet.set("name", "张三");
    packet.setInt("age", 25);
    packet.setDouble("score", 95.5);
    
    ASSERT_EQ(packet.get("name"), "张三");
    ASSERT_EQ(packet.getInt("age"), 25);
    ASSERT_TRUE(packet.getDouble("score") > 95.0 && packet.getDouble("score") < 96.0);
    ASSERT_EQ(packet.get("not_exist", "default"), "default");
    ASSERT_EQ(packet.getInt("not_exist", -1), -1);
}

// 测试Channel基本订阅发布
TEST(channel_basic_pubsub) {
    Channel channel("test");
    channel.start();
    
    std::atomic<int> received{0};
    std::string lastTopic;
    
    SubscriptionId id = channel.subscribe("user.created", [&](const DataPacket& p) {
        received++;
        lastTopic = p.topic;
    });
    
    ASSERT_TRUE(id > 0);
    ASSERT_EQ(channel.subscriberCount(), 1u);
    
    channel.publish("user.created", "plugin1", {{"name", "李四"}});
    
    // 等待异步处理
    for (int i = 0; i < 100 && received.load() == 0; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ASSERT_EQ(received.load(), 1);
    ASSERT_EQ(lastTopic, "user.created");
    
    channel.unsubscribe(id);
    ASSERT_EQ(channel.subscriberCount(), 0u);
    
    channel.stop();
}

// 测试通配符订阅
TEST(channel_wildcard_subscription) {
    Channel channel("test");
    channel.start();
    
    std::atomic<int> allCount{0};
    std::atomic<int> specificCount{0};
    
    channel.subscribe("*", [&](const DataPacket& p) {
        allCount++;
    });
    
    channel.subscribe("specific.event", [&](const DataPacket& p) {
        specificCount++;
    });
    
    channel.publish("specific.event", "src", {});
    channel.publish("other.event", "src", {});
    
    for (int i = 0; i < 100 && allCount.load() < 2; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ASSERT_EQ(allCount.load(), 2);
    ASSERT_EQ(specificCount.load(), 1);
    
    channel.stop();
}

// 测试同步发布
TEST(channel_sync_publish) {
    Channel channel("test");
    // 不启动异步线程
    
    std::atomic<int> received{0};
    
    channel.subscribe("sync.test", [&](const DataPacket& p) {
        received++;
    });
    
    DataPacket packet;
    packet.topic = "sync.test";
    packet.source = "test";
    
    channel.publishSync(packet);
    
    // 同步发布应该立即执行
    ASSERT_EQ(received.load(), 1);
}

// 测试ChannelManager
TEST(channel_manager) {
    auto& mgr = ChannelManager::instance();
    
    auto ch1 = mgr.get("channel1");
    ASSERT_TRUE(ch1 != nullptr);
    ASSERT_EQ(ch1->name(), "channel1");
    
    auto ch1Again = mgr.get("channel1");
    ASSERT_TRUE(ch1.get() == ch1Again.get());  // 应该是同一个实例
    
    auto defaultCh = mgr.getDefault();
    ASSERT_TRUE(defaultCh != nullptr);
    ASSERT_EQ(defaultCh->name(), "default");
    
    mgr.remove("channel1");
    auto names = mgr.names();
    bool found = false;
    for (const auto& n : names) {
        if (n == "channel1") found = true;
    }
    ASSERT_TRUE(!found);
    
    mgr.closeAll();
}

// 测试多订阅者
TEST(channel_multiple_subscribers) {
    Channel channel("test");
    channel.start();
    
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};
    std::atomic<int> count3{0};
    
    channel.subscribe("event", [&](const DataPacket& p) { count1++; });
    channel.subscribe("event", [&](const DataPacket& p) { count2++; });
    channel.subscribe("event", [&](const DataPacket& p) { count3++; });
    
    channel.publish("event", "src", {});
    
    for (int i = 0; i < 100 && (count1.load() < 1 || count2.load() < 1 || count3.load() < 1); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ASSERT_EQ(count1.load(), 1);
    ASSERT_EQ(count2.load(), 1);
    ASSERT_EQ(count3.load(), 1);
    
    channel.stop();
}

// 测试数据传递
TEST(channel_data_transmission) {
    Channel channel("test");
    channel.start();
    
    std::string receivedName;
    int receivedAge = 0;
    
    channel.subscribe("user.data", [&](const DataPacket& p) {
        receivedName = p.get("name");
        receivedAge = p.getInt("age");
    });
    
    DataPacket packet;
    packet.topic = "user.data";
    packet.source = "plugin";
    packet.set("name", "王五");
    packet.setInt("age", 30);
    
    channel.publish(packet);
    
    for (int i = 0; i < 100 && receivedAge == 0; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ASSERT_EQ(receivedName, "王五");
    ASSERT_EQ(receivedAge, 30);
    
    channel.stop();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Channel 单元测试" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    RUN_TEST(data_packet_basic);
    RUN_TEST(channel_basic_pubsub);
    RUN_TEST(channel_wildcard_subscription);
    RUN_TEST(channel_sync_publish);
    RUN_TEST(channel_manager);
    RUN_TEST(channel_multiple_subscribers);
    RUN_TEST(channel_data_transmission);
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  测试结果: 通过 " << tests_passed << "/" << (tests_passed + tests_failed) << std::endl;
    std::cout << "========================================" << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
