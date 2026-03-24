# 工作流插件系统集成指南

## 📋 概述

本文档说明如何将插件系统与工作流系统融合，使插件可以作为工作流节点执行。

## 🎯 设计目标

1. **插件作为工作流节点** - 插件可以直接作为工作流中的可执行节点
2. **上下文共享** - 插件可以访问工作流上下文，读取输入、产生输出
3. **生命周期管理** - 统一管理插件的加载、初始化、执行、卸载
4. **热插拔** - 支持运行时动态加载和卸载插件
5. **类型安全** - 插件接口提供类型安全的参数传递

## 📐 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     工作流系统                                │
│  ┌───────────────┐       ┌───────────────┐                  │
│  │  工作流图      │──────▶│  插件管理器    │                  │
│  └───────────────┘       └───────┬───────┘                  │
│                                  │                          │
│                                  ▼                          │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              工作流插件节点                            │  │
│  │  ┌─────────────┐    ┌───────────────┐                │  │
│  │  │ 插件上下文   │◀───│ 工作流上下文   │                │  │
│  │  └──────┬──────┘    └───────────────┘                │  │
│  │         │                                          │  │
│  │         ▼                                          │  │
│  │  ┌──────────────────────────────────────────────┐  │  │
│  │  │         插件实现 (IWorkflowPlugin)            │  │  │
│  │  │  - execute()                                 │  │  │
│  │  │  - validateParams()                          │  │  │
│  │  │  - getSupportedActions()                     │  │  │
│  │  └──────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     插件系统                                │
│  ┌───────────────┐       ┌───────────────┐                  │
│  │  插件加载器    │──────▶│  服务注册表    │                  │
│  └───────────────┘       └───────────────┘                  │
└─────────────────────────────────────────────────────────────┘
```

## 🔧 核心组件

### 1. IWorkflowPlugin - 工作流插件接口

所有工作流插件必须实现此接口：

```cpp
class IWorkflowPlugin : public Plugin::IPlugin {
public:
    // 执行插件（作为工作流节点）
    virtual Plugin::PluginResult execute(
        IWorkflowPluginContext* context,
        const Plugin::PluginParams& params
    ) = 0;
    
    // 验证参数
    virtual bool validateParams(const Plugin::PluginParams& params) const = 0;
    
    // 获取支持的动作列表
    virtual std::vector<std::string> getSupportedActions() const = 0;
    
    // 获取参数模板
    virtual std::string getActionParamTemplate(const std::string& action) const = 0;
};
```

### 2. IWorkflowPluginContext - 工作流插件上下文

融合了工作流上下文和插件上下文：

```cpp
class IWorkflowPluginContext : public Plugin::IPluginContext {
public:
    // 工作流上下文功能
    virtual std::shared_ptr<IWorkflowContext> getWorkflowContext() const = 0;
    virtual std::string getCurrentNodeId() const = 0;
    virtual std::string getWorkflowName() const = 0;
    
    // 节点数据传递
    virtual void setOutput(const std::string& key, const std::string& value) = 0;
    virtual std::string getInput(const std::string& key, const std::string& defaultValue = "") const = 0;
    
    // 执行控制
    virtual void requestPause() = 0;
    virtual void requestStop() = 0;
    virtual void requestSkip() = 0;
};
```

### 3. IWorkflowPluginManager - 插件管理器

负责插件生命周期管理：

```cpp
class IWorkflowPluginManager {
public:
    // 加载/卸载
    virtual std::string loadPlugin(const std::string& pluginPath) = 0;
    virtual bool unloadPlugin(const std::string& pluginName) = 0;
    virtual bool reloadPlugin(const std::string& pluginName) = 0;
    virtual int loadPluginsFromDirectory(const std::string& directory) = 0;
    
    // 查询
    virtual std::shared_ptr<IWorkflowPlugin> getPlugin(const std::string& pluginName) const = 0;
    virtual bool hasPlugin(const std::string& pluginName) const = 0;
    virtual std::vector<std::string> getLoadedPlugins() const = 0;
    
    // 执行
    virtual Plugin::PluginResult executePlugin(
        const std::string& pluginName,
        IWorkflowPluginContext* context,
        const Plugin::PluginParams& params
    ) = 0;
};
```

## 📝 使用示例

### 步骤1: 创建工作流插件

```cpp
// my_data_processor_plugin.cpp
#include "workflow_system/interfaces/workflow_plugin.h"

class MyDataProcessorPlugin : public WorkflowSystem::IWorkflowPlugin {
public:
    // 初始化插件
    Plugin::PluginResult initialize(IPluginContext* context) override {
        context->logInfo("数据处理器插件初始化");
        return Plugin::PluginResult::ok("初始化成功");
    }
    
    // 执行插件
    Plugin::PluginResult execute(
        IWorkflowPluginContext* context,
        const Plugin::PluginParams& params
    ) override {
        // 1. 从工作流获取输入数据
        std::string inputData = context->getInput("data", "");
        
        // 2. 处理数据
        std::string action = params.action;
        if (action == "transform") {
            std::string result = transformData(inputData);
            
            // 3. 设置输出数据
            context->setOutput("result", result);
            
            return Plugin::PluginResult::ok("处理成功", result);
        }
        
        return Plugin::PluginResult::error(1, "未知的动作: " + action);
    }
    
    // 获取元数据
    PluginMetadata getMetadata() const override {
        PluginMetadata meta;
        meta.name = "my_data_processor";
        meta.displayName = "数据处理器";
        meta.description = "处理和转换数据";
        meta.version = PluginVersion(1, 0, 0);
        meta.author = "Your Name";
        return meta;
    }
    
    // 获取支持的动作
    std::vector<std::string> getSupportedActions() const override {
        return {"transform", "validate", "compress"};
    }
    
    // 验证参数
    bool validateParams(const Plugin::PluginParams& params) const override {
        return !params.action.empty();
    }
    
private:
    std::string transformData(const std::string& data) {
        // 实际的数据处理逻辑
        return "processed:" + data;
    }
};

// 导出插件工厂函数
extern "C" {
    WorkflowSystem::IWorkflowPlugin* createPlugin() {
        return new MyDataProcessorPlugin();
    }
    
    void destroyPlugin(WorkflowSystem::IWorkflowPlugin* plugin) {
        delete plugin;
    }
}
```

### 步骤2: 在工作流中使用插件

```cpp
// workflow_with_plugin.cpp
#include "workflow_system/interfaces/workflow_plugin.h"
#include "workflow_system/implementations/workflow_orchestrator.h"

int main() {
    // 1. 创建插件管理器
    auto pluginManager = WorkflowSystem::createWorkflowPluginManager();
    
    // 2. 加载插件
    std::string pluginName = pluginManager->loadPlugin("./plugins/my_data_processor.so");
    if (pluginName.empty()) {
        std::cerr << "插件加载失败" << std::endl;
        return 1;
    }
    
    // 3. 初始化插件
    pluginManager->initializeAll();
    
    // 4. 创建工作流
    auto orchestrator = std::make_shared<WorkflowOrchestrator>();
    auto context = std::make_shared<WorkflowContext>();
    
    // 5. 创建工作流图，添加插件节点
    auto graph = std::make_shared<WorkflowGraph>();
    graph->addNode("node1", "plugin:" + pluginName);
    
    // 6. 设置插件节点配置
    Plugin::PluginParams params;
    params.action = "transform";
    params.args["input"] = "test_data";
    
    // 7. 执行工作流
    WorkflowPluginContext pluginCtx(context, "node1", "my_workflow");
    auto result = pluginManager->executePlugin(pluginName, &pluginCtx, params);
    
    if (result.success) {
        std::cout << "插件执行成功: " << result.message << std::endl;
        std::cout << "输出数据: " << pluginCtx.getInput("result") << std::endl;
    } else {
        std::cerr << "插件执行失败: " << result.message << std::endl;
    }
    
    // 8. 清理
    pluginManager->unloadAll();
    
    return 0;
}
```

## 🗂️ 目录结构

```
workflow_system_project/
├── include/
│   └── workflow_system/
│       ├── interfaces/
│       │   └── workflow_plugin.h          # 插件集成接口
│       └── implementations/
│           └── workflow_plugins/
│               ├── workflow_plugin_context.h   # 上下文实现
│               ├── workflow_plugin_manager.h   # 管理器接口
│               └── workflow_plugin_manager.cpp # 管理器实现
├── plugins/
│   ├── examples/
│   │   ├── data_processor_plugin.cpp     # 示例插件
│   │   └── notification_plugin.cpp       # 通知插件
│   └── build/                            # 插件编译输出
│       └── *.so                          # 动态链接库
└── examples/
    └── workflow_with_plugin_example.cpp  # 使用示例
```

## 🔨 编译插件

```bash
# 编译插件为动态库
g++ -shared -fPIC -std=c++11 \
    -I../include \
    -o libmy_processor.so \
    my_data_processor_plugin.cpp

# 或使用CMake
cd plugins
mkdir build && cd build
cmake ..
make
```

## 📊 配置文件示例

`workflow_config.json`:
```json
{
  "workflow_name": "数据处理流程",
  "nodes": [
    {
      "id": "data_loader",
      "type": "builtin",
      "action": "load_file",
      "params": {
        "file_path": "/data/input.txt"
      }
    },
    {
      "id": "processor",
      "type": "plugin",
      "plugin_name": "my_data_processor",
      "plugin_path": "./plugins/libmy_processor.so",
      "action": "transform",
      "params": {
        "mode": "uppercase",
        "timeout": 30
      }
    },
    {
      "id": "saver",
      "type": "builtin",
      "action": "save_file",
      "params": {
        "file_path": "/data/output.txt"
      }
    }
  ],
  "edges": [
    ["data_loader", "processor"],
    ["processor", "saver"]
  ]
}
```

## ⚙️ 高级特性

### 1. 插件热重载

```cpp
// 在不停止工作流的情况下重载插件
pluginManager->reloadPlugin("my_data_processor");
```

### 2. 插件依赖管理

```cpp
// 插件元数据中声明依赖
PluginMetadata meta;
meta.dependencies = {"data_validator", "logger"};
```

### 3. 插件通信

```cpp
// 插件间通过服务注册表通信
context->registerService("data_store", myDataStore);
void* service = context->getService("data_store");
```

### 4. 错误处理和重试

```cpp
WorkflowPluginNodeConfig config;
config.continueOnError = true;  // 出错时继续执行下一个节点
config.timeoutSeconds = 60;     // 超时时间
```

## 🔍 调试技巧

### 启用详细日志

```cpp
Logger::instance().setLevel(LogLevel::DEBUG);
context->logDebug("当前处理: " + data);
```

### 检查插件状态

```cpp
auto metadata = pluginManager->getPluginMetadata("my_plugin");
std::cout << "插件版本: " << metadata.version.toString() << std::endl;
std::cout << "插件状态: " << (int)plugin->getState() << std::endl;
```

### 列出已加载插件

```cpp
auto plugins = pluginManager->getLoadedPlugins();
for (const auto& name : plugins) {
    std::cout << "已加载: " << name << std::endl;
}
```

## 📚 最佳实践

1. **插件职责单一** - 每个插件只做一件事并做好
2. **充分测试** - 在独立环境中测试插件
3. **错误处理** - 妥善处理所有可能的错误情况
4. **文档完整** - 为每个插件提供清晰的文档
5. **版本管理** - 使用语义化版本号
6. **资源管理** - 确保插件正确释放资源
7. **线程安全** - 如果插件会被多线程调用，确保线程安全

## 🎓 总结

通过插件系统与工作流系统的集成，你可以：

✅ 动态扩展工作流功能
✅ 不修改核心代码添加新功能
✅ 提高代码可维护性
✅ 支持第三方插件开发
✅ 实现功能模块化

相关文档:
- [项目目录结构](PROJECT_STRUCTURE.md)
- [功能特性](FEATURES.md)
- [开发路线图](ROADMAP.md)
