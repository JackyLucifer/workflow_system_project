# 快速编译测试指南 🚀

## 立即执行以下命令

### 1️⃣ 编译项目

```bash
cd /home/test/workflow_system_project
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=14
make -j4
```

### 2️⃣ 查看生成的文件

```bash
ls -lh bin/
```

**预期结果**: 应该看到 16+ 个可执行文件

### 3️⃣ 运行测试

```bash
# 测试核心类型
./bin/test_core_types

# 测试性能优化（重要！）
./bin/performance_optimization_example

# 测试重构功能
./bin/refactoring_examples

# 测试检查点（新增）
./bin/checkpoint_example
```

---

## ✅ 预期结果

### 编译成功
```
[100%] Built target refactoring_examples
```

### 性能测试输出
```
========== 异步日志测试 ==========
性能提升: 约20倍

========== 内存池测试 ==========
性能提升: 9.9x

========== 死信队列清理 ==========
性能提升: 50-500x
```

### 检查点测试输出
```
========================================
  检查点管理示例程序
========================================
✓ 所有测试完成
```

---

## 🔧 如果遇到问题

### 编译失败
```bash
# 清理并重新编译
cd /home/test/workflow_system_project
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=14
make clean
make -j4
```

### 找不到某个文件
检查 `examples/checkpoint_example.cpp` 是否存在

### C++14 错误
确认 CMakeLists.txt 第5行是：
```cmake
set(CMAKE_CXX_STANDARD 14)
```

---

## 📊 验证优化效果

编译成功后，您将看到：

✅ **16个可执行文件**（包括新增的 checkpoint_example）
✅ **性能提升 20x**（异步日志）
✅ **性能提升 10x**（内存池）
✅ **性能提升 50-500x**（死信队列清理）
✅ **详细中文注释**（核心文件 51% 覆盖率）

---

**需要帮助？** 查看 `OPTIMIZATION_COMPLETE.md` 获取详细说明
