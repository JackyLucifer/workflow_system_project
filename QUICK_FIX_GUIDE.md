# 快速修复指南 🚀

**立即修复的3个高优先级问题**

---

## 🔴 问题1: checkpoint_example.cpp 缺失

### 症状
```bash
# 编译时会报错
make: *** No rule to make target 'examples/checkpoint_example.cpp'
```

### 解决方案：创建文件

创建 `/home/test/workflow_system_project/examples/checkpoint_example.cpp`:

```cpp
#include <iostream>
#include <workflow_system/interfaces/checkpoint_manager.h>
#include <workflow_system/interfaces/workflow.h>
#include <workflow_system/core/logger.h>

using namespace WorkflowSystem;

class SimpleWorkflow : public BaseWorkflow {
public:
    SimpleWorkflow() : BaseWorkflow("checkpoint_example") {}

    void execute(const IMessage& message) override {
        LOG_INFO("Executing workflow with message: " + message.getContent());

        // 模拟工作流步骤
        LOG_INFO("Step 1: Processing data");
        LOG_INFO("Step 2: Saving results");
        LOG_INFO("Step 3: Cleaning up");

        markAsCompleted();
    }
};

int main() {
    std::cout << "========================================\n";
    std::cout << "  检查点管理示例程序\n";
    std::cout << "========================================\n\n";

    // 创建检查点管理器
    auto checkpointMgr = createCheckpointManager();

    // 创建工作流
    auto workflow = std::make_shared<SimpleWorkflow>();

    std::cout << "========== 测试1: 基本执行 ==========\n";
    workflow->execute(Message("test_message"));

    std::cout << "\n========== 测试2: 保存检查点 ==========\n";
    if (checkpointMgr) {
        checkpointMgr->saveCheckpoint(workflow, "checkpoint_1");
        std::cout << "检查点已保存\n";
    }

    std::cout << "\n========== 测试3: 列出检查点 ==========\n";
    if (checkpointMgr) {
        auto checkpoints = checkpointMgr->listCheckpoints();
        std::cout << "找到 " << checkpoints.size() << " 个检查点\n";
        for (const auto& cp : checkpoints) {
            std::cout << "  - " << cp << "\n";
        }
    }

    std::cout << "\n========== 测试4: 恢复检查点 ==========\n";
    if (checkpointMgr) {
        auto restored = checkpointMgr->restoreCheckpoint("checkpoint_1", workflow);
        if (restored) {
            std::cout << "工作流已从检查点恢复\n";
        }
    }

    std::cout << "\n========== 测试5: 清理检查点 ==========\n";
    if (checkpointMgr) {
        checkpointMgr->clearOldCheckpoints(0);  // 清理所有检查点
        std::cout << "旧检查点已清理\n";
    }

    std::cout << "\n========================================\n";
    std::cout << "  所有测试完成！\n";
    std::cout << "========================================\n";

    return 0;
}
```

---

## 🔴 问题2: C++11 vs C++14 冲突

### 症状
```
error: 'shared_mutex' is not a member of 'std'
```

### 解决方案：修改 CMakeLists.txt

编辑 `/home/test/workflow_system_project/CMakeLists.txt` 第5行：

**原代码**:
```cmake
set(CMAKE_CXX_STANDARD 11)
```

**修改为**:
```cmake
set(CMAKE_CXX_STANDARD 14)
```

---

## 🔴 问题3: 根目录文档散乱

### 症状
根目录有7个 .md 文件应该移到 docs/ 目录

### 解决方案：执行移动命令

```bash
cd /home/test/workflow_system_project

# 移动文档到 docs/ 目录
mv BUILD_AND_TEST.md docs/
mv REFACTORING_QUICK_REFERENCE.md docs/
mv CLEANUP_GUIDE.md docs/
mv CLEANUP_PLAN.md docs/
mv CODE_COMMENT_SUMMARY.md docs/
mv COMPILE_TEST_GUIDE.md docs/
mv OPTIMIZATION_SUGGESTIONS.md docs/
```

---

## ✅ 验证修复

执行以下命令验证修复：

```bash
cd /home/test/workflow_system_project

# 1. 清理旧的构建
rm -rf build

# 2. 创建新的构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake .. -DCMAKE_CXX_STANDARD=14

# 4. 编译
make -j4

# 5. 查看生成的可执行文件
ls -lh bin/

# 6. 运行检查点示例
./bin/checkpoint_example

# 7. 运行性能优化示例
./bin/performance_optimization_example

# 8. 运行重构示例
./bin/refactoring_examples
```

---

## 📋 其他建议（可选）

### 中优先级
1. 为核心文件添加详细注释（logging_macros.h, exceptions.h）
2. 更新 README.md 添加性能指标
3. 创建 CHANGELOG.md
4. 扩展单元测试

### 低优先级
1. 配置 Doxygen 生成 API 文档
2. 创建架构图
3. 设置 CI/CD
4. 添加性能基准测试

详细说明请参考 `OPTIMIZATION_SUGGESTIONS.md`

---

*快速修复指南 v1.0*
*更新日期: 2026-03-23*
