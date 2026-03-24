#!/bin/bash

# 编译和测试脚本

set -e

echo "========================================="
echo "  工作流系统编译和测试脚本"
echo "========================================="
echo ""

PROJECT_ROOT="/home/test/workflow_system_project"
BUILD_DIR="$PROJECT_ROOT/build"

cd "$BUILD_DIR"

echo "步骤1: 清理旧的构建..."
rm -rf *
echo "✓ 清理完成"
echo ""

echo "步骤2: 配置CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release
echo "✓ CMake配置完成"
echo ""

echo "步骤3: 编译项目..."
make -j4
echo "✓ 编译完成"
echo ""

echo "步骤4: 测试性能优化示例..."
echo "-----------------------------------"
./bin/performance_optimization_example
echo "-----------------------------------"
echo ""

echo "步骤5: 测试检查点管理器..."
echo "-----------------------------------"
./bin/checkpoint_example
echo "-----------------------------------"
echo ""

echo "========================================="
echo "  所有测试完成！"
echo "========================================="
