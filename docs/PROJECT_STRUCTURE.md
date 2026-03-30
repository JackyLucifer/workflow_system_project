# 项目结构说明

```
workflow_system_project/
│
├── include/workflow_system/        # 📦 公共头文件（用户只需 include 这个目录）
│   ├── core/                       #    核心工具
│   │   ├── any.h                   #      类型擦除容器
│   │   ├── exceptions.h            #      自定义异常
│   │   ├── logger.h                #      日志系统
│   │   ├── logging_macros.h        #      日志宏
│   │   ├── types.h                 #      基础类型定义
│   │   └── utils.h                 #      工具函数
│   ├── interfaces/                 #    纯虚接口（20个接口）
│   └── implementations/            #    实现类头文件
│
├── src/implementations/            # 🔧 实现文件
│   ├── sqlite_workflow_persistence.cpp
│   ├── json_config_manager.cpp
│   ├── workflow_orchestrator.cpp
│   ├── workflow_manager.cpp
│   ├── workflow_graph.cpp
│   ├── workflow_scheduler_impl.cpp
│   ├── workflow_template_impl.cpp
│   ├── workflow_version_control_impl.cpp
│   ├── workflow_tracer_impl.cpp
│   ├── conditional_workflow_impl.cpp
│   ├── alert_manager_impl.cpp
│   ├── dead_letter_queue_impl.cpp
│   ├── metrics_collector.cpp
│   ├── async_logger.cpp
│   ├── system_facade.cpp
│   └── ...
│
├── examples/                       # 📖 示例程序（15个）
│   ├── main.cpp                    #    基础工作流示例
│   ├── config_example.cpp          #    配置管理
│   ├── orchestration_example.cpp   #    工作流编排
│   ├── persistence_example.cpp     #    持久化
│   ├── scheduler_example.cpp       #    调度
│   ├── observer_example.cpp        #    观察者
│   └── ...
│
├── tests/                          # ✅ 单元测试（15个测试文件，240+用例）
│   ├── test_framework.h            #    自定义测试框架
│   ├── test_all.cpp                #    核心综合测试
│   ├── test_core_types.cpp         #    类型系统
│   └── ...
│
├── plugins/                        # 🔌 插件子系统（独立）
│   ├── include/                    #    插件接口
│   ├── src/                        #    插件加载器
│   ├── examples/                   #    插件示例
│   ├── tests/                      #    插件测试
│   └── CMakeLists.txt              #    独立构建配置
│
├── CMakeLists.txt                  # 🔨 主构建配置
└── README.md                       # 📄 项目文档
```

## 构建产物

编译后在 `build/` 目录下生成：

```
build/
├── bin/                            # 可执行文件
│   ├── test_*                      #    测试程序（15个）
│   ├── workflow_example            #    示例程序（15个）
│   └── ...
└── lib/
    └── libworkflow_system.a        # 静态库
```
