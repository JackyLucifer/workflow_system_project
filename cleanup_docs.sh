#!/bin/bash
# 文档整理脚本 - 将根目录的文档移动到 docs/ 目录

echo "开始整理文档..."

cd /home/test/workflow_system_project

# 移动文档到 docs/
mv BUILD_AND_TEST.md docs/ 2>/dev/null && echo "✓ BUILD_AND_TEST.md"
mv REFACTORING_QUICK_REFERENCE.md docs/ 2>/dev/null && echo "✓ REFACTORING_QUICK_REFERENCE.md"
mv CLEANUP_GUIDE.md docs/ 2>/dev/null && echo "✓ CLEANUP_GUIDE.md"
mv CLEANUP_PLAN.md docs/ 2>/dev/null && echo "✓ CLEANUP_PLAN.md"
mv CODE_COMMENT_SUMMARY.md docs/ 2>/dev/null && echo "✓ CODE_COMMENT_SUMMARY.md"
mv COMPILE_TEST_GUIDE.md docs/ 2>/dev/null && echo "✓ COMPILE_TEST_GUIDE.md"
mv OPTIMIZATION_SUGGESTIONS.md docs/ 2>/dev/null && echo "✓ OPTIMIZATION_SUGGESTIONS.md"
mv QUICK_FIX_GUIDE.md docs/ 2>/dev/null && echo "✓ QUICK_FIX_GUIDE.md"

echo "文档整理完成！"
