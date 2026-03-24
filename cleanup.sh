#!/bin/bash

# 工作流系统清理脚本
# 删除重复和多余的文件

PROJECT_ROOT="/home/test/workflow_system_project"
cd "$PROJECT_ROOT"

echo "========================================="
echo "  清理多余和重复的文件"
echo "========================================="
echo ""

# ========== 1. 删除 include/ 中的实现文件 ==========
echo "步骤1: 删除 include/workflow_system/implementations/ ..."
echo "实现文件应该在 src/implementations/，不应该在公共API中"

# 检查目录是否存在
if [ -d "include/workflow_system/implementations" ]; then
    echo "发现以下文件："
    ls -la include/workflow_system/implementations/
    echo ""
    echo "删除整个目录..."
    rm -rf include/workflow_system/implementations
    echo "✓ 已删除 include/workflow_system/implementations/"
else
    echo "目录不存在，跳过"
fi

echo ""

# ========== 2. 删除 src/ 中重复的接口文件 ==========
echo "步骤2: 删除 src/ 中与 include/ 重复的接口文件..."

# 检查 src/interfaces/ 是否存在
if [ -d "src/interfaces" ]; then
    echo "发现 src/interfaces/ 目录"
    echo "保留：workflow_plugin.h（扩展接口）"
    echo ""

    # 只保留特殊的扩展接口
    cd src/interfaces

    # 删除与 include/ 重复的文件
    for file in *.h; do
        if [ -f "$file" ]; then
            # 检查 include/ 中是否存在同名文件
            if [ -f "$PROJECT_ROOT/include/workflow_system/interfaces/$file" ]; then
                if [ "$file" != "workflow_plugin.h" ]; then
                    echo "  删除重复文件: $file"
                    rm "$file"
                fi
            fi
        fi
    done

    cd "$PROJECT_ROOT"

    # 如果目录为空或只有特殊文件，删除整个目录
    if [ -d "src/interfaces" ]; then
        remaining=$(ls src/interfaces/ 2>/dev/null | wc -l)
        if [ "$remaining" -le 1 ]; then
            echo "✓ src/interfaces/ 已清空或只包含扩展接口，删除目录"
            # 保留 workflow_plugin.h 到其他地方
            if [ -f "src/interfaces/workflow_plugin.h" ]; then
                mv src/interfaces/workflow_plugin.h src/implementations/workflow_plugins/
            fi
            rm -rf src/interfaces
        else
            echo "⚠ src/interfaces/ 还有其他文件，保留目录"
        fi
    fi
else
    echo "src/interfaces/ 不存在，跳过"
fi

echo ""

# ========== 3. 删除 src/core/ 中重复的文件 ==========
echo "步骤3: 删除 src/core/ 中与 include/ 重复的文件..."

if [ -d "src/core" ]; then
    echo "发现 src/core/ 目录"

    cd src/core

    # 删除与 include/ 重复的文件
    for file in types.h any.h utils.h logger.h; do
        if [ -f "$file" ] && [ -f "$PROJECT_ROOT/include/workflow_system/core/$file" ]; then
            echo "  删除重复文件: $file"
            rm "$file"
        fi
    done

    cd "$PROJECT_ROOT"

    # 如果目录为空，删除它
    if [ -d "src/core" ]; then
        remaining=$(ls src/core/ 2>/dev/null | wc -l)
        if [ "$remaining" -eq 0 ]; then
            echo "✓ src/core/ 已清空，删除目录"
            rm -rf src/core
        else
            echo "⚠ src/core/ 还有其他文件，保留目录"
            echo "  保留的文件："
            ls src/core/
        fi
    fi
else
    echo "src/core/ 不存在，跳过"
fi

echo ""

# ========== 4. 删除未使用的实现文件 ==========
echo "步骤4: 删除未使用的旧实现..."

# 删除 alert_manager_impl.h（未使用，我们用 alert_manager_simple.h）
if [ -f "src/implementations/alert_manager_impl.h" ]; then
    echo "  删除未使用: src/implementations/alert_manager_impl.h"
    rm src/implementations/alert_manager_impl.h
fi

# 删除 dead_letter_queue.h（未使用，我们用 dead_letter_queue_impl.h）
if [ -f "src/implementations/dead_letter_queue.h" ]; then
    echo "  删除未使用: src/implementations/dead_letter_queue.h"
    rm src/implementations/dead_letter_queue.h
fi

echo ""

# ========== 5. 整理根目录的文档 ==========
echo "步骤5: 整理文档..."

# 移动根目录的文档到 docs/
if [ -f "BUILD_AND_TEST.md" ]; then
    echo "  移动 BUILD_AND_TEST.md 到 docs/"
    mv BUILD_AND_TEST.md docs/
fi

if [ -f "REFACTORING_QUICK_REFERENCE.md" ]; then
    echo "  移动 REFACTORING_QUICK_REFERENCE.md 到 docs/"
    mv REFACTORING_QUICK_REFERENCE.md docs/
fi

echo ""

# ========== 6. 删除空的 plugins 目录（如果存在） ==========
echo "步骤6: 检查 plugins/ 目录..."

if [ -d "plugins" ]; then
    # 检查是否有实际内容
    file_count=$(find plugins -type f | wc -l)
    if [ "$file_count" -eq 0 ]; then
        echo "  plugins/ 为空，删除目录"
        rm -rf plugins
    else
        echo "  plugins/ 有内容，保留"
        echo "  文件数: $file_count"
    fi
fi

echo ""

# ========== 总结 ==========
echo "========================================="
echo "  清理完成！"
echo "========================================="
echo ""
echo "删除的内容："
echo "  ✓ include/workflow_system/implementations/ (不应在公共API中)"
echo "  ✓ src/ 中与 include/ 重复的接口文件"
echo "  ✓ src/ 中与 include/ 重复的核心文件"
echo "  ✓ 未使用的旧实现文件"
echo "  ✓ 重复的文档已移动到 docs/"
echo ""

# 显示清理后的结构
echo "当前目录结构："
echo "  include/workflow_system/"
echo "    ├── core/        (公共核心API)"
echo "    ├── interfaces/  (公共接口定义)"
echo "    └── ...          (其他公共文件)"
echo "  src/"
echo "    ├── implementations/ (实现文件)"
echo "    └── ...              (内部文件)"
echo "  examples/             (示例程序)"
echo "  tests/               (测试文件)"
echo "  docs/                (文档)"
echo ""
