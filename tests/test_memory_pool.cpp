#include <cstdio>
#include "workflow_system/core/logger.h"
/**
 * @file test_memory_pool.cpp
 * @brief 内存池测试
 */

#include "workflow_system/implementations/memory_pool.h"
#include "test_framework.h"
#include <vector>
#include <thread>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试类型 ==========

struct TestData {
    int value;
    double data;
    char name[64];

    TestData() : value(0), data(0.0) {
        name[0] = '\0';
    }

    TestData(int v, double d, const std::string& n) : value(v), data(d) {
        snprintf(name, sizeof(name), "%s", n.c_str());
    }
};

// ========== 测试用例 ==========

void testMemoryPool_Construction() {
    MemoryPool<TestData, 100> pool;

    auto stats = pool.getStats();

    TEST_ASSERT_TRUE(stats.allocatedBlocks > 0);
    TEST_ASSERT_TRUE(stats.totalMemoryBytes > 0);
}

void testMemoryPool_Allocate() {
    MemoryPool<TestData, 100> pool;

    TestData* data = pool.allocate();

    TEST_ASSERT_TRUE(data != nullptr);

    // 使用 placement new 构造对象
    new(data) TestData(42, 3.14, "test");

    TEST_ASSERT_EQUAL(42, data->value);
    TEST_ASSERT_FLOAT_EQUAL(3.14, data->data, 0.001);

    // 手动析构
    data->~TestData();
}

void testMemoryPool_Deallocate() {
    MemoryPool<TestData, 100> pool;

    size_t beforeDealloc = pool.getAvailableCount();
    TestData* data = pool.allocate();
    new(data) TestData(42, 3.14, "test");

    // 析构对象
    data->~TestData();

    // 归还内存
    pool.deallocate(data);

    TEST_ASSERT_TRUE(pool.getAvailableCount() == beforeDealloc);
}

void testMemoryPool_MultipleAllocations() {
    MemoryPool<TestData, 10> pool;

    std::vector<TestData*> items;

    for (int i = 0; i < 5; ++i) {
        TestData* data = pool.allocate();
        TEST_ASSERT_TRUE(data != nullptr);
        new(data) TestData(i, i * 1.5, "item_" + std::to_string(i));
        items.push_back(data);
    }

    // 验证数据
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL(i, items[i]->value);
    }

    // 清理
    for (auto* data : items) {
        data->~TestData();
        pool.deallocate(data);
    }
}

void testMemoryPool_GetAvailableCount() {
    MemoryPool<TestData, 10> pool;

    size_t initialCount = pool.getAvailableCount();

    TestData* data1 = pool.allocate();
    size_t afterAlloc1 = pool.getAvailableCount();

    TEST_ASSERT_TRUE(afterAlloc1 < initialCount);

    TestData* data2 = pool.allocate();
    size_t afterAlloc2 = pool.getAvailableCount();

    TEST_ASSERT_TRUE(afterAlloc2 < afterAlloc1);

    pool.deallocate(data1);
    pool.deallocate(data2);
}

void testMemoryPool_Clear() {
    MemoryPool<TestData, 10> pool;

    TestData* data1 = pool.allocate();
    TestData* data2 = pool.allocate();

    TEST_ASSERT_TRUE(pool.getStats().allocatedBlocks > 0);

    pool.clear();

    auto stats = pool.getStats();

    TEST_ASSERT_EQUAL(0, static_cast<int>(stats.allocatedBlocks));
    TEST_ASSERT_EQUAL(0, static_cast<int>(stats.totalMemoryBytes));
}

void testMemoryPool_Defragment() {
    MemoryPool<TestData, 10> pool;

    // 这个测试主要验证不会崩溃
    pool.defragment();

    TEST_ASSERT_TRUE(true);
}

void testMemoryPool_GetStats() {
    MemoryPool<TestData, 100> pool;

    auto stats = pool.getStats();

    TEST_ASSERT_TRUE(stats.allocatedBlocks > 0);
    TEST_ASSERT_TRUE(stats.totalMemoryBytes > 0);
    TEST_ASSERT_TRUE(stats.availableCount > 0);
}

void testMemoryPool_SetMaxMemoryLimit() {
    MemoryPool<TestData, 10> pool(1024);  // 1KB 限制

    pool.setMaxMemoryLimit(2048);

    auto stats = pool.getStats();

    TEST_ASSERT_EQUAL(2048, static_cast<int>(stats.maxMemoryLimit));
}

void testMemoryPool_AllocateDeallocateCycle() {
    MemoryPool<TestData, 10> pool;

    // 多次分配和释放
    for (int cycle = 0; cycle < 3; ++cycle) {
        std::vector<TestData*> items;

        for (int i = 0; i < 5; ++i) {
            TestData* data = pool.allocate();
            TEST_ASSERT_TRUE(data != nullptr);
            new(data) TestData(i, 0.0, "");
            items.push_back(data);
        }

        for (auto* data : items) {
            data->~TestData();
            pool.deallocate(data);
        }
    }

    // 最终应该有可用的内存块
    TEST_ASSERT_TRUE(pool.getAvailableCount() > 0);
}

void testMemoryPool_MemoryLimit() {
    // 使用足够大的限制允许至少一次扩容
    MemoryPool<TestData, 10> pool(10000);

    std::vector<TestData*> items;
    TestData* data;
    int count = 0;

    while ((data = pool.allocate()) != nullptr && count < 50) {
        new(data) TestData(count, 0.0, "");
        items.push_back(data);
        count++;
    }

    // 清理
    for (auto* item : items) {
        item->~TestData();
        pool.deallocate(item);
    }

    TEST_ASSERT_TRUE(count > 0);
}

void testMemoryPool_ThreadSafety() {
    MemoryPool<TestData, 100> pool;

    std::vector<std::thread> threads;
    std::vector<std::vector<TestData*>> threadItems(10);

    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 10; ++i) {
                TestData* data = pool.allocate();
                if (data) {
                    new(data) TestData(i, 0.0, "");
                    threadItems[t].push_back(data);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 清理
    for (auto& items : threadItems) {
        for (auto* data : items) {
            data->~TestData();
            pool.deallocate(data);
        }
    }

    TEST_ASSERT_TRUE(true);
}

// ========== ObjectPool 测试用例 ==========

void testObjectPool_Acquire() {
    ObjectPool<TestData> pool(5);

    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();

    TEST_ASSERT_TRUE(obj1 != nullptr);
    TEST_ASSERT_TRUE(obj2 != nullptr);
}

void testObjectPool_AutoReturn() {
    ObjectPool<TestData> pool(5);

    {
        auto obj = pool.acquire();
        obj->value = 42;
    }  // 对象自动归还到池

    TEST_ASSERT_EQUAL(5, static_cast<int>(pool.size()));
}

void testObjectPool_EmptyPool() {
    ObjectPool<TestData> pool(1);

    auto obj1 = pool.acquire();
    auto obj2 = pool.acquire();  // 池为空，会创建新对象

    TEST_ASSERT_TRUE(obj1 != nullptr);
    TEST_ASSERT_TRUE(obj2 != nullptr);
}

void testObjectPool_ThreadSafety() {
    ObjectPool<TestData> pool(5);

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            auto obj = pool.acquire();
            if (obj) {
                obj->value = 42;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 所有对象都应该归还到池
    TEST_ASSERT_EQUAL(5, static_cast<int>(pool.size()));
}

// ========== 测试套件创建 ==========

TestSuite createMemoryPoolTestSuite() {
    TestSuite suite("内存池测试");

    suite.addTest("MemoryPool_Construction", testMemoryPool_Construction);
    suite.addTest("MemoryPool_Allocate", testMemoryPool_Allocate);
    suite.addTest("MemoryPool_Deallocate", testMemoryPool_Deallocate);
    suite.addTest("MemoryPool_MultipleAllocations", testMemoryPool_MultipleAllocations);
    suite.addTest("MemoryPool_GetAvailableCount", testMemoryPool_GetAvailableCount);
    suite.addTest("MemoryPool_Clear", testMemoryPool_Clear);
    suite.addTest("MemoryPool_Defragment", testMemoryPool_Defragment);
    suite.addTest("MemoryPool_GetStats", testMemoryPool_GetStats);
    suite.addTest("MemoryPool_SetMaxMemoryLimit", testMemoryPool_SetMaxMemoryLimit);
    suite.addTest("MemoryPool_AllocateDeallocateCycle", testMemoryPool_AllocateDeallocateCycle);
    suite.addTest("MemoryPool_MemoryLimit", testMemoryPool_MemoryLimit);
    suite.addTest("MemoryPool_ThreadSafety", testMemoryPool_ThreadSafety);

    suite.addTest("ObjectPool_Acquire", testObjectPool_Acquire);
    suite.addTest("ObjectPool_AutoReturn", testObjectPool_AutoReturn);
    suite.addTest("ObjectPool_EmptyPool", testObjectPool_EmptyPool);
    suite.addTest("ObjectPool_ThreadSafety", testObjectPool_ThreadSafety);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createMemoryPoolTestSuite());

    return TestRunner::runAllSuites(suites);
}
