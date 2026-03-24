# 项目目录结构说明

## 标准C++项目结构

本项目采用标准的C++项目目录布局，符合现代C++项目的最佳实践。

```
workflow_system_project/
├── CMakeLists.txt           # CMake构建配置
├── README.md                # 项目说明文档
├── .gitignore              # Git忽略文件配置
│
├── include/                # 公共头文件（对外接口）
│   └── workflow_system/
│       ├── core/           # 核心工具类
│       │   ├── any.h       # 通用类型包装
│       │   ├── logger.h    # 日志系统
│       │   ├── types.h     # 类型定义
│       │   └── utils.h     # 工具函数
│       │
│       ├── interfaces/     # 接口定义
│       │   ├── workflow.h  # 工作流接口
│       │   ├── workflow_manager.h
│       │   ├── workflow_graph.h
│       │   ├── workflow_observer.h
│       │   ├── workflow_orchestrator.h
│       │   ├── workflow_persistence.h
│       │   ├── workflow_template.h
│       │   ├── workflow_scheduler.h
│       │   ├── workflow_tracer.h
│       │   ├── workflow_version_control.h
│       │   ├── conditional_workflow.h
│       │   ├── alert_manager.h
│       │   ├── checkpoint_manager.h
│       │   ├── dead_letter_queue.h
│       │   ├── resource_manager.h
│       │   ├── timeout_handler.h
│       │   ├── retry_policy.h
│       │   ├── metrics_collector.h
│       │   └── config_manager.h
│       │
│       └── implementations/ # 具体实现
│           ├── workflows/
│           │   └── base_workflow.h
│           ├── workflow_context.h
│           ├── filesystem_resource_manager.h
│           ├── system_facade.h
│           ├── workflow_manager.h
│           ├── timeout_handler.h
│           ├── retry_policy.h
│           ├── metrics_collector.h
│           ├── json_config_manager.h
│           ├── workflow_graph.h
│           ├── workflow_observer_impl.h
│           ├── workflow_orchestrator.h
│           ├── sqlite_workflow_persistence.h
│           ├── workflow_version_control_impl.h
│           ├── conditional_workflow_impl.h
│           ├── workflow_template_impl.h
│           ├── workflow_scheduler_impl.h
│           ├── workflow_tracer_impl.h
│           ├── alert_manager_impl.h
│           ├── checkpoint_manager_impl.h
│           ├── dead_letter_queue_impl.h
│           └── circuit_breaker.h
│
├── src/                    # 实现源文件（非header-only部分）
│   ├── core/               # 核心头文件（保留）
│   ├── interfaces/         # 接口头文件（保留）
│   └── implementations/    # .cpp实现文件
│       ├── json_config_manager.cpp
│       └── sqlite_workflow_persistence.cpp
│
├── examples/               # 示例程序
│   ├── main.cpp           # 主示例
│   ├── workflow_example.cpp
│   ├── queue_example.cpp
│   ├── advanced_example.cpp
│   ├── config_example.cpp
│   ├── orchestration_example.cpp
│   ├── observer_example.cpp
│   ├── comprehensive_example.cpp
│   ├── persistence_example.cpp
│   ├── version_control_example.cpp
│   ├── conditional_example.cpp
│   ├── workflow_template_example.cpp
│   └── scheduler_example.cpp
│
├── tests/                  # 测试文件
│   ├── test_framework.h
│   ├── test_all.cpp
│   └── test_core_types.cpp
│
├── docs/                   # 文档
│   ├── FEATURES.md        # 功能概览
│   ├── ROADMAP.md         # 开发路线图
│   └── PROJECT_STRUCTURE.md # 本文档
│
├── plugins/               # 插件系统（独立构建）
│   ├── include/          # 插件接口
│   ├── src/              # 插件实现
│   ├── examples/         # 插件示例
│   ├── tests/            # 插件测试
│   └── CMakeLists.txt    # 插件构建配置
│
└── build/                 # 构建输出目录（自动生成）
    ├── bin/              # 可执行文件
    │   ├── workflow_example
    │   ├── queue_example
    │   └── ...
    └── lib/              # 库文件
```

## 目录说明

### `/include` - 公共头文件

**用途**: 存放对外暴露的所有公共头文件，是用户使用库时需要包含的文件。

**结构**: 使用 `include/workflow_system/` 作为根命名空间，避免与其他库的头文件冲突。

**使用方式**:
```cpp
#include "workflow_system/interfaces/workflow.h"
#include "workflow_system/implementations/system_facade.h"
```

### `/src` - 源文件

**用途**: 存放非header-only的实现文件（.cpp文件）。

**说明**: 
- 大部分实现是header-only的（.h文件包含实现）
- 只有少数需要编译的实现才放在src/implementations/*.cpp
- `src/core/`、`src/interfaces/`、`src/implementations/`下的.h文件是为了内部开发方便保留的副本

### `/examples` - 示例程序

**用途**: 展示如何使用工作流系统的各种功能。

**编译**: 所有示例都会编译到 `build/bin/examples/` 目录。

### `/tests` - 测试文件

**用途**: 单元测试和集成测试。

**运行测试**:
```bash
cd build
make test
# 或
./bin/tests/test_all
```

### `/docs` - 文档

**用途**: 项目文档、设计文档、开发指南等。

### `/plugins` - 插件系统

**用途**: 可选的插件扩展系统，有自己的独立构建配置。

**构建**:
```bash
cd plugins
mkdir build && cd build
cmake ..
make
```

### `/build` - 构建输出

**用途**: CMake生成的构建文件和编译输出。

**说明**: 
- 该目录由CMake自动生成
- 可以随时删除并重新构建
- 已在`.gitignore`中配置，不会被Git跟踪

## CMake目标

### 可执行文件

所有示例程序都会编译为独立的可执行文件：
- `workflow_example`
- `queue_example`
- `advanced_example`
- `config_example`
- `orchestration_example`
- `observer_example`
- `comprehensive_example`
- `persistence_example`
- `version_control_example`
- `conditional_example`
- `workflow_template_example`
- `scheduler_example`

### 测试目标

- `test_all` - 运行所有单元测试
- `test_core_types` - 核心类型测试

### 快捷目标

- `make test` - 运行所有测试
- `make run_tests` - 运行主测试套件

## 安装布局

执行 `make install` 后，文件会安装到系统目录：

```
/usr/local/
├── include/
│   └── workflow_system/    # 所有头文件
├── bin/                     # 可选的示例程序
└── lib/
    └── pkgconfig/
        └── workflow_system.pc
```

## 迁移指南

如果你使用旧版本的项目结构，需要进行以下调整：

### 1. 头文件路径变化

**旧版本**:
```cpp
#include "interfaces/workflow.h"
```

**新版本**:
```cpp
#include "workflow_system/interfaces/workflow.h"
```

### 2. CMakeLists.txt更新

确保你的CMakeLists.txt使用新的头文件路径：

```cmake
# 旧版本
include_directories(${CMAKE_SOURCE_DIR}/src)

# 新版本
include_directories(${CMAKE_SOURCE_DIR}/include)
target_link_libraries(your_target PRIVATE workflow_system)
```

### 3. 示例程序位置

**旧版本**: `src/`目录下
**新版本**: `examples/`目录下

## 最佳实践

1. **包含头文件**: 始终使用 `#include "workflow_system/xxx.h"` 格式
2. **链接库**: 使用 `target_link_libraries(your_target PRIVATE workflow_system)`
3. **开发新功能**: 
   - 接口放在 `include/workflow_system/interfaces/`
   - 实现放在 `include/workflow_system/implementations/`
   - 如果有.cpp源文件，放在 `src/implementations/`
4. **添加示例**: 新示例放在 `examples/` 目录
5. **添加测试**: 新测试放在 `tests/` 目录

## 项目优势

采用这种标准结构的好处：

✅ **清晰分离**: 公共接口和内部实现明确分离
✅ **易于集成**: 符合CMake和包管理器的标准布局
✅ **便于维护**: 开发者可以快速找到需要修改的文件
✅ **header-only友好**: 大部分实现可以直接在头文件中
✅ **支持混合模式**: 可以同时包含header-only和需要编译的实现
✅ **标准化**: 遵循现代C++项目的目录组织最佳实践
