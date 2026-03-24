# WorkflowSystem

一个轻量级、可扩展的 C++ 工作流管理框架，采用头文件库形式，方便集成到各种项目中。

## 特性

- **头文件库**：所有实现都在头文件中，无需编译库文件
- **可扩展**：通过继承 `BaseWorkflow` 轻松创建自定义工作流
- **协议无关**：通用消息接口，支持任何消息协议（MQTT、HTTP、WebSocket等）
- **线程安全**：使用互斥锁保护共享资源
- **资源管理**：自动管理工作流生命周期中的资源
- **观察者模式**：支持监听工作流状态变化
- **超时控制**：支持工作流级和消息级超时管理
- **重试机制**：支持多种重试策略（固定延迟、指数退避、线性退避）
- **指标收集**：自动收集工作流执行指标，支持多种报告格式

## 目录

- [项目结构](#项目结构)
- [核心类详解](#核心类详解)
  - [ILogger & ConsoleLogger](#ilogger--consolelogger)
  - [工具类 (Utils)](#工具类-utils)
  - [类型定义 (Types)](#类型定义-types)
- [接口类详解](#接口类详解)
  - [IWorkflowObserver](#iworkflowobserver)
  - [IResourceManager & Resource](#iresourcemanager--resource)
  - [IWorkflowContext](#iworkflowcontext)
  - [IMessage](#imessage)
  - [IWorkflow](#iworkflow)
  - [IWorkflowManager](#iworkflowmanager)
  - [ITimeoutHandler](#itimeouthandler)
  - [IRetryPolicy](#iretrypolicy)
  - [IMetricsCollector](#imetricscollector)
- [实现类详解](#实现类详解)
  - [WorkflowContext](#workflowcontext)
  - [FileSystemResourceManager](#filesystemresourcemanager)
  - [BaseWorkflow & Observable](#baseworkflow--observable)
  - [WorkflowManager](#workflowmanager)
  - [SystemFacade](#systemfacade)
  - [TimeoutHandler](#timeouthandler)
  - [RetryPolicy](#retrypolicy)
  - [MetricsCollector](#metricscollector)
- [高级功能](#高级功能)
  - [超时控制](#超时控制)
  - [重试机制](#重试机制)
  - [指标收集](#指标收集)
- [快速开始](#快速开始)
- [设计模式](#设计模式)
- [编译与运行](#编译与运行)

---

## 项目结构

```
workflow_system_project/
├── CMakeLists.txt              # 构建配置
├── README.md                  # 本文档
└── src/
    ├── core/                   # 核心模块
    │   ├── types.h           # 枚举类型定义
    │   ├── logger.h          # 日志系统（单例）
    │   └── utils.h           # 工具类（ID生成、时间、字符串）
    ├── interfaces/              # 接口层
    │   ├── workflow.h         # 工作流接口 + IMessage + IWorkflowContext
    │   ├── workflow_manager.h # 工作流管理器接口
    │   ├── resource_manager.h  # 资源管理器接口
    │   ├── workflow_observer.h # 观察者接口
    │   ├── timeout_handler.h # 超时处理器接口
    │   ├── retry_policy.h    # 重试策略接口
    │   └── metrics_collector.h # 指标收集器接口
    └── implementations/         # 实现层
        ├── system_facade.h     # 系统门面（单例）
        ├── workflow_manager.h   # 工作流管理器实现
        ├── workflow_context.h  # 工作流上下文实现
        ├── filesystem_resource_manager.h # 文件系统资源管理器
        ├── timeout_handler.h   # 超时处理器实现
        ├── retry_policy.h      # 重试策略实现
        ├── metrics_collector.h # 指标收集器实现
        └── workflows/
            └── base_workflow.h  # 工作流抽象基类
```

---

## 核心类详解

### ILogger & ConsoleLogger

#### 位置
- 接口：`src/core/logger.h`
- 实现：`src/core/logger.h`（同文件）

#### 功能与用处
提供统一的日志记录功能，支持不同日志级别。

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `info(msg)` | 记录信息级别日志 | void |
| `warning(msg)` | 记录警告级别日志 | void |
| `error(msg)` | 记录错误级别日志 | void |

#### 用法
```cpp
// 1. 使用便捷宏（推荐）
#include "core/logger.h"

LOG_INFO("Workflow started");
LOG_WARNING("No context available");
LOG_ERROR("Failed to create resource");

// 2. 直接使用 Logger 接口
#include "core/logger.h"

auto& logger = WorkflowSystem::ConsoleLogger::getInstance();
logger.info("Workflow started");
logger.warning("No context available");
logger.error("Failed to create resource");
```

#### 注意事项
- 使用宏定义时自动添加前缀 `[INFO]`、`[WARNING]`、`[ERROR]`
- 线程安全：内部使用互斥锁保护输出
- 单例模式：全局唯一实例

---

### 工具类 (Utils)

#### 位置
`src/core/utils.h`

#### IdGenerator

**功能**：生成唯一的资源ID

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `generate()` | 生成格式为 `id_timestamp_counter` 的唯一ID | `std::string` |

**用例**：
```cpp
std::string id1 = WorkflowSystem::IdGenerator::generate();
std::string id2 = WorkflowSystem::IdGenerator::generate();
// id1 = "id_1234567890000_0"
// id2 = "id_1234567890000_1"
```

#### TimeUtils

**功能**：提供时间相关的辅助方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `getCurrentTimestamp()` | 获取当前时间戳（格式：YYYYMMDD_HHMMSS） | `std::string` |
| `getCurrentMs()` | 获取当前毫秒时间戳 | `int64_t` |

**用例**：
```cpp
std::string timestamp = WorkflowSystem::TimeUtils::getCurrentTimestamp();
// "20240312_143025"

int64_t ms = WorkflowSystem::TimeUtils::getCurrentMs();
// 1709745025000
```

#### StringUtils

**功能**：提供字符串处理的辅助方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `contains(str, substr)` | 检查字符串是否包含子串 | `bool` |
| `joinPath(base, relative)` | 连接路径 | `std::string` |

**用例**：
```cpp
bool found = WorkflowSystem::StringUtils::contains("hello world", "world");
// true

std::string path = WorkflowSystem::StringUtils::joinPath("/base", "relative");
// "/base/relative"
```

---

### 类型定义 (Types)

#### 位置
`src/core/types.h`

#### 功能与用处
定义工作流状态和资源类型的枚举类型。

#### WorkflowState（工作流状态）

| 状态 | 值 | 说明 |
|------|------|------|
| `IDLE` | 0 | 空闲状态 |
| `RUNNING` | 1 | 运行中 |
| `INTERRUPTED` | 2 | 已中断 |
| `COMPLETED` | 3 | 已完成 |
| `ERROR` | 4 | 错误状态 |

#### ResourceType（资源类型）

| 类型 | 值 | 说明 |
|------|------|------|
| `DIRECTORY` | 0 | 目录资源 |
| `FILE` | 1 | 文件资源 |
| `TEMPORARY` | 2 | 临时资源 |

#### 工具函数

| 函数 | 功能 | 参数 | 返回值 |
|------|--------|------|------|
| `workflowStateToString(state)` | 状态转字符串 | `WorkflowState` | `std::string` |
| `resourceTypeToString(type)` | 类型转字符串 | `ResourceType` | `std::string` |

**用例**：
```cpp
#include "core/types.h"

WorkflowSystem::WorkflowState state = WorkflowSystem::WorkflowState::RUNNING;
std::string stateStr = WorkflowSystem::workflowStateToString(state);
// "running"

WorkflowSystem::ResourceType type = WorkflowSystem::ResourceType::DIRECTORY;
std::string typeStr = WorkflowSystem::resourceTypeToString(type);
// "directory"
```

---

## 接口类详解

### IWorkflowObserver

#### 位置
`src/interfaces/workflow_observer.h`

#### 功能与用处
定义工作流观察者接口，用于监听工作流状态变化。

#### 接口方法

| 方法 | 功能 | 参数 |
|------|--------|------|
| `onWorkflowStarted(name)` | 工作流启动时调用 | `const std::string& workflowName` |
| `onWorkflowFinished(name)` | 工作流完成时调用 | `const std::string& workflowName` |
| `onWorkflowInterrupted(name)` | 工作流中断时调用 | `const std::string& workflowName` |
| `onWorkflowError(name, error)` | 工作流出错时调用 | `const std::string& workflowName`, `const std::string& error` |

#### 用法

```cpp
#include "interfaces/workflow_observer.h"

class MyObserver : public WorkflowSystem::IWorkflowObserver {
public:
    void onWorkflowStarted(const std::string& name) override {
        std::cout << "Workflow started: " << name << std::endl;
        // 记录开始时间、更新UI等
    }

    void onWorkflowFinished(const std::string& name) override {
        std::cout << "Workflow finished: " << name << std::endl;
        // 记录完成时间、计算执行耗时等
    }

    void onWorkflowInterrupted(const std::string& name) override {
        std::cout << "Workflow interrupted: " << name << std::endl;
        // 处理中断后的清理工作
    }

    void onWorkflowError(const std::string& name,
                      const std::string& error) override {
        std::cout << "Workflow error: " << name << " - " << error << std::endl;
        // 发送告警、记录错误日志等
    }
};

// 添加观察者
workflow->addObserver(std::make_shared<MyObserver>());

// 移除观察者
workflow->removeObserver(observer);
```

---

### IResourceManager & Resource

#### 位置
`src/interfaces/resource_manager.h`

#### 功能与用处
定义资源管理接口，管理工作流使用的资源（目录、文件等）。

#### Resource 类（资源实体）

封装资源的所有属性。

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `getId()` | 获取资源ID | `const std::string&` |
| `getType()` | 获取资源类型 | `ResourceType` |
| `getPath()` | 获取资源路径 | `const std::string&` |
| `setMetadata(key, value)` | 设置元数据 | `void` |
| `getMetadata(key)` | 获取元数据 | `std::string` |

**用例**：
```cpp
auto resource = std::make_shared<WorkflowSystem::Resource>(
    "id_123", WorkflowSystem::ResourceType::DIRECTORY, "/path/to/dir"
);

resource->setMetadata("size", "1024");
std::string size = resource->getMetadata("size");
// "1024"
```

#### IResourceManager 接口

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `createDirectory(path)` | 创建目录资源 | `shared_ptr<Resource>` |
| `createDirectory()` | 创建临时目录 | `shared_ptr<Resource>` |
| `getResource(id)` | 获取指定ID的资源 | `shared_ptr<Resource>` |
| `cleanup(id)` | 清理指定资源 | `bool` |
| `cleanupAll()` | 清理所有资源 | `size_t` |
| `getAllResources()` | 获取所有资源 | `vector<shared_ptr<Resource>>` |

---

### IWorkflowContext

#### 位置
`src/interfaces/workflow.h`

#### 功能与用处
定义工作流上下文接口，用于在工作流之间传递数据和资源。
支持字符串和任意类型对象的存储。

#### 接口方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `set(key, value)` | 设置键值对（字符串） | `void` |
| `get(key, defaultValue)` | 获取值（支持默认值） | `std::string` |
| `setObject<T>(key, value)` | 设置对象（支持任意类型） | `void` |
| `getObject<T>(key)` | 获取对象 | `T` |
| `getObject<T>(key, defaultValue)` | 获取对象（带默认值） | `T` |
| `has(key)` | 检查键是否存在 | `bool` |
| `getWorkingDir()` | 获取工作目录 | `std::string` |
| `setWorkingDir(path)` | 设置工作目录（注册为资源） | `void` |
| `cleanup()` | 清理关联的资源 | `void` |
| `getAllData()` | 获取所有字符串数据 | `unordered_map<string, string>` |
| `getAllObjects()` | 获取所有对象（类型擦除） | `unordered_map<string, any>` |

#### 对象存储说明

**核心特性**：
- 使用 C++17 的 `std::any` 存储任意类型
- 支持复杂对象传递（自定义类、结构体、STL容器等）
- 线程安全：内部使用互斥锁保护

**用例**：

```cpp
auto ctx = workflow->getContext();

// ========== 字符串数据存储 ==========
ctx->set("user_id", "user_001");
ctx->set("session_id", "session_123");

std::string userId = ctx->get("user_id", "default");
std::string sessionId = ctx->get("session_id");

// ========== 对象存储 ==========

// 1. 存储简单类型
ctx->setObject("count", 42);
int count = ctx->getObject<int>("count");
// count = 42

ctx->setObject("rate", 3.14);
double rate = ctx->getObject<double>("rate");
// rate = 3.14

// 2. 存储自定义结构体
struct UserProfile {
    std::string name;
    int age;
    std::vector<std::string> hobbies;
};

UserProfile profile = {
    .name = "Alice",
    .age = 25,
    .hobbies = {"reading", "coding", "gaming"}
};

ctx->setObject("profile", profile);
UserProfile retrievedProfile = ctx->getObject<UserProfile>("profile");

// 3. 存储容器类型
std::vector<int> numbers = {1, 2, 3, 4, 5};
ctx->setObject("numbers", numbers);

std::vector<int> retrieved = ctx->getObject<std::vector<int>>("numbers");
// retrieved = {1, 2, 3, 4, 5}

// 4. 存储智能指针
auto data = std::make_shared<std::string>("important data");
ctx->setObject("shared_data", data);

auto retrieved = ctx->getObject<std::shared_ptr<std::string>>("shared_data");

// 5. 带默认值获取
auto maybeInt = ctx->getObject<int>("missing_key", -1);
// 如果键不存在，返回 -1

// ========== 检查键是否存在 ==========
if (ctx->has("profile")) {
    // 键存在
}

// ========== 移除对象 ==========
ctx->removeObject("profile");

// ========== 获取所有对象 ==========
auto allObjects = ctx->getAllObjects();
for (const auto& [key, value] : allObjects) {
    // value 是 std::any 类型
    // 可通过 type().name() 检查实际类型
}

// ========== 工作目录管理 ==========
ctx->setWorkingDir("/path/to/working/dir");
std::string workDir = ctx->getWorkingDir();

// ========== 获取所有数据 ==========
auto allData = ctx->getAllData();
for (const auto& [key, value] : allData) {
    std::cout << key << " = " << value << std::endl;
}

// ========== 清理（会自动删除工作目录） ==========
ctx->cleanup();
```

**在自定义工作流中使用对象**：

```cpp
class DataProcessingWorkflow : public WorkflowSystem::BaseWorkflow {
public:
    DataProcessingWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("DataProcessing", resourceManager) {}

protected:
    void onStart() override {
        auto ctx = getContext();

        // 从上下文获取输入数据
        auto data = ctx->getObject<std::vector<int>>("input_data");
        auto config = ctx->getObject<Config>("config");

        // 处理数据
        process(data, config);

        // 存储处理结果
        ctx->setObject("result", processedData);
    }

    void onMessage(const IMessage& message) override {
        auto ctx = getContext();

        // 从消息中提取数据并存储
        auto payload = parsePayload(message.getPayload());
        ctx->setObject("message_data", payload);
    }
};
```

**传递多个对象到工作流**：

在启动工作流之前可以预先设置多个对象：

```cpp
auto ctx = manager->getWorkflow("MyWorkflow")->getContext();

// 批量设置对象
ctx->setObject<int>("threshold", 100);
ctx->setObject<double>("factor", 0.85);
ctx->setObject<bool>("enabled", true);
ctx->setObject<std::string>("mode", "fast");

// 然后启动工作流，工作流可以直接使用这些对象
manager->startWorkflow("MyWorkflow");
```


---

### IMessage

#### 位置
`src/interfaces/workflow.h`

#### 功能与用处
通用消息接口，协议无关，支持不同类型的消息协议。

#### 接口方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `getType()` | 获取消息类型 | `std::string` |
| `getTopic()` | 获取消息主题/路由 | `std::string` |
| `getPayload()` | 获取消息内容 | `std::string` |

**用例**：
```cpp
// MQTT 消息
class MqttMessage : public WorkflowSystem::IMessage {
public:
    MqttMessage(const std::string& topic, const std::string& payload)
        : type_("mqtt"), topic_(topic), payload_(payload) {}

    std::string getType() const override { return type_; }
    std::string getTopic() const override { return topic_; }
    std::string getPayload() const override { return payload_; }
private:
    std::string type_, topic_, payload_;
};

// HTTP 消息
class HttpMessage : public WorkflowSystem::IMessage {
public:
    HttpMessage(const std::string& method, const std::string& body)
        : type_("http"), topic_(method), body_(body) {}

    std::string getType() const override { return type_; }
    std::string getTopic() const override { return topic_; }
    std::string getPayload() const override { return body_; }
private:
    std::string type_, topic_, body_;
};
```

---

### IWorkflow

#### 位置
`src/interfaces/workflow.h`

#### 功能与用处
工作流接口，定义工作流的基本行为和生命周期。

#### 接口方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `getName()` | 获取工作流名称 | `std::string` |
| `getState()` | 获取当前状态 | `WorkflowState` |
| `start(context)` | 启动工作流 | `void` |
| `handleMessage(msg)` | 处理消息 | `void` |
| `interrupt()` | 中断工作流 | `void` |
| `finish()` | 完成工作流 | `void` |
| `error(msg)` | 设置错误状态 | `void` |
| `setContext(ctx)` | 设置上下文 | `void` |
| `getContext()` | 获取上下文 | `shared_ptr<IWorkflowContext>` |
| `addObserver(obs)` | 添加观察者 | `void` |
| `removeObserver(obs)` | 移除观察者 | `void` |
| `setMessageHandler(handler)` | 设置自定义消息处理器 | `void` |

**观察者管理说明**：

- **`addObserver(observer)`**：注册一个监听器，在工作流状态变化时接收通知
- **`removeObserver(observer)`**：移除已注册的监听器，停止接收状态通知

**典型应用场景**：
- **日志记录**：记录工作流的执行历史
- **UI 更新**：实时显示工作流状态
- **监控告警**：在工作流出错时发送告警
- **统计收集**：收集工作流执行时间、成功率等指标

---

### IWorkflowManager

#### 位置
`src/interfaces/workflow_manager.h`

#### 功能与用处
工作流管理器接口，负责工作流的注册、启动、切换和消息分发。

#### 接口方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `registerWorkflow(name, workflow)` | 注册工作流 | `bool` |
| `getWorkflow(name)` | 获取工作流 | `shared_ptr<IWorkflow>` |
| `startWorkflow(name)` | 启动工作流（创建新上下文） | `bool` |
| `switchWorkflow(name, preserveCtx)` | 切换工作流（可选保留上下文） | `bool` |
| `interrupt()` | 中断当前工作流 | `void` |
| `handleMessage(msg)` | 处理消息（转发给当前工作流） | `bool` |
| `cleanupAllResources()` | 清理所有资源 | `void` |
| `getCurrentWorkflow()` | 获取当前工作流 | `shared_ptr<IWorkflow>` |
| `getStatus()` | 获取状态信息（JSON格式） | `std::string` |
| `getResourceManager()` | 获取资源管理器 | `shared_ptr<IResourceManager>` |

**用例**：
```cpp
auto manager = system.getManager();

// 注册工作流
auto workflow = std::make_shared<MyWorkflow>(resourceManager);
manager->registerWorkflow("MyWorkflow", workflow);

// 获取工作流
auto wf = manager->getWorkflow("MyWorkflow");

// 启动工作流
manager->startWorkflow("MyWorkflow");

// 切换工作流（保留上下文）
manager->switchWorkflow("AnotherWorkflow", true);

// 处理消息
auto msg = MyMessage("topic", "payload");
manager->handleMessage(msg);

// 中断工作流
manager->interrupt();

// 获取状态
std::string status = manager->getStatus();
// {"current_workflow": "MyWorkflow", "workflow_state": "running", ...}

// 清理所有资源
manager->cleanupAllResources();
```

---

## 实现类详解

### WorkflowContext

#### 位置
`src/implementations/workflow_context.h`

#### 功能与用处
工作流上下文实现，负责在工作流之间传递数据和资源引用。

#### 用法
```cpp
// 通过 SystemFacade 获取
auto& system = WorkflowSystem::SystemFacade::getInstance();
system.initialize("/path/to/resources");

// 上下文由工作流管理器自动创建和传递
// 使用示例见 IWorkflowContext 部分
```

---

### FileSystemResourceManager

#### 位置
`src/implementations/filesystem_resource_manager.h`

#### 功能与用处
文件系统资源管理器实现，使用 `std::filesystem` 管理文件系统资源。

#### 公开方法

| 方法 | 功能 | 说明 |
|------|--------|------|
| `createDirectory(path)` | 创建指定路径的目录 | 自动生成资源ID |
| `createDirectory()` | 创建临时目录 | 目录名带时间戳 |
| `getResource(id)` | 获取指定ID的资源 | 返回 nullptr 如果不存在 |
| `cleanup(id)` | 清理指定资源 | 如果是目录则删除其内容 |
| `cleanupAll()` | 清理所有资源 | 返回清理的资源数量 |
| `getAllResources()` | 获取所有资源 | 返回所有资源列表 |
| `getBasePath()` | 获取基础路径 | 用于资源管理的根路径 |

#### 用法
```cpp
// 获取资源管理器
auto resourceManager = system.getResourceManager();

// 创建工作目录
auto workDir = resourceManager->createDirectory("/path/to/work");
std::cout << "Directory ID: " << workDir->getId() << std::endl;
std::cout << "Directory Path: " << workDir->getPath() << std::endl;

// 创建临时目录
auto tempDir = resourceManager->createDirectory();

// 获取资源
auto resource = resourceManager->getResource(workDir->getId());

// 清理资源
resourceManager->cleanup(workDir->getId());

// 清理所有资源
size_t count = resourceManager->cleanupAll();
```

---

### BaseWorkflow & Observable

#### 位置
`src/implementations/workflows/base_workflow.h`

#### 功能与用处
工作流抽象基类，定义工作流的生命周期算法骨架。

#### Observable 类（观察者基类）

| 方法 | 功能 |
|------|--------|
| `addObserver(observer)` | 添加观察者 |
| `removeObserver(observer)` | 移除观察者 |
| `notifyStarted(name)` | 通知所有观察者工作流启动 |
| `notifyFinished(name)` | 通知所有观察者工作流完成 |
| `notifyInterrupted(name)` | 通知所有观察者工作流中断 |
| `notifyError(name, error)` | 通知所有观察者工作流出错 |

#### BaseWorkflow 类

**公共方法**（IWorkflow 接口实现）：

| 方法 | 功能 |
|------|--------|
| `getName()` | 获取工作流名称 |
| `getState()` | 获取工作流状态 |
| `setContext(ctx)` | 设置上下文 |
| `getContext()` | 获取上下文 |
| `addObserver(obs)` | 添加观察者 |
| `removeObserver(obs)` | 移除观察者 |
| `setMessageHandler(handler)` | 设置自定义消息处理器 |

**生命周期方法**：

| 方法 | 功能 | 触发时机 |
|------|--------|----------|
| `start(context)` | 启动工作流 | 调用 `onStart()` + 通知观察者 |
| `handleMessage(msg)` | 处理消息 | 先调用自定义处理器，再调用 `onMessage()` |
| `interrupt()` | 中断工作流 | 调用 `onInterrupt()` + 通知观察者 |
| `finish()` | 完成工作流 | 调用 `onFinalize()` + 通知观察者 |
| `error(msg)` | 设置错误状态 | 记录错误日志 + 通知观察者 |

**需要子类实现的钩子方法**：

| 方法 | 功能 | 必须实现 |
|------|--------|------------|
| `onStart()` | 工作流启动时调用 | 是 |
| `onMessage(msg)` | 接收消息时调用 | 是 |
| `onInterrupt()` | 工作流中断时调用 | 是 |
| `onFinalize()` | 工作流完成时调用 | 否（默认空实现） |

#### 用法

```cpp
#include "implementations/workflows/base_workflow.h"

class MyWorkflow : public WorkflowSystem::BaseWorkflow {
public:
    MyWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("MyWorkflow", resourceManager) {}

protected:
    void onStart() override {
        // 初始化逻辑
        auto ctx = getContext();
        if (ctx) {
            ctx->set("status", "initialized");
        }
    }

    void onMessage(const IMessage& message) override {
        // 消息处理逻辑
        if (message.getTopic() == "command") {
            ctx->set("last_command", message.getPayload());
        }
    }

    void onInterrupt() override {
        // 清理逻辑
    }

    void onFinalize() override {
        // 完成后处理
        // 保存结果、发送通知等
    }
};
```

---

### WorkflowManager

#### 位置
`src/implementations/workflow_manager.h`

#### 功能与用处
工作流管理器实现，管理工作流的注册、启动、切换和消息分发。

#### 用法
```cpp
// 通过 SystemFacade 获取
auto manager = system.getManager();

// 注册工作流
manager->registerWorkflow("MyWorkflow",
    std::static_pointer_cast<IWorkflow>(myWorkflow));

// 启动工作流
manager->startWorkflow("MyWorkflow");

// 切换工作流（保留上下文）
manager->switchWorkflow("AnotherWorkflow", true);

// 中断工作流
manager->interrupt();

// 获取状态（JSON格式）
std::string status = manager->getStatus();
```

---

### SystemFacade

#### 位置
`src/implementations/system_facade.h`

#### 功能与用处
系统门面，为整个工作流管理系统提供统一的访问接口。

#### 方法

| 方法 | 功能 | 返回值 |
|------|--------|---------|
| `getInstance()` | 获取单例实例 | `static SystemFacade&` |
| `initialize(basePath)` | 初始化系统 | `void` |
| `getManager()` | 获取工作流管理器 | `shared_ptr<IWorkflowManager>` |
| `getResourceManager()` | 获取资源管理器 | `shared_ptr<IResourceManager>` |
| `shutdown()` | 关闭系统，清理所有资源 | `void` |

#### 用法
```cpp
#include "implementations/system_facade.h"

using namespace WorkflowSystem;

int main() {
    // 1. 获取单例
    auto& system = SystemFacade::getInstance();

    // 2. 初始化系统
    system.initialize("/path/to/resources");

    // 3. 获取管理器
    auto manager = system.getManager();
    auto resourceManager = system.getResourceManager();

    // 4. 使用工作流系统
    auto workflow = std::make_shared<MyWorkflow>(resourceManager);
    manager->registerWorkflow("MyWorkflow",
        std::static_pointer_cast<IWorkflow>(workflow));
    manager->startWorkflow("MyWorkflow");

    // 5. 关闭系统
    system.shutdown();

    return 0;
}
```

---

## 高级功能

### ITimeoutHandler

#### 位置
- 接口：`src/interfaces/timeout_handler.h`
- 实现：`src/implementations/timeout_handler.h`

#### 功能与用处
超时处理器接口，用于管理工作流执行超时。

#### 超时策略 (TimeoutPolicy)

| 策略 | 值 | 说明 |
|------|------|------|
| `INTERRUPT` | 中断工作流 | 超时后立即中断工作流 |
| `PAUSE` | 暂停工作流 | 超时后暂停工作流 |
| `RETRY` | 重试工作流 | 超时后自动重试 |
| `NOTIFY_ONLY` | 仅发送通知 | 超时后仅通知，不中断工作流 |

#### 用法
```cpp
#include "implementations/timeout_handler.h"

// 创建超时处理器
auto timeoutHandler = TimeoutHandlerFactory::createWithConfig(
    "MyWorkflow",
    workflow,
    TimeoutConfig{
        .timeoutMs = 5000,           // 5秒超时
        .policy = TimeoutPolicy::INTERRUPT,
        .callback = [](const std::string& name, int64_t elapsed) {
            std::cout << "Workflow " << name << " timed out after "
                      << elapsed << "ms" << std::endl;
        },
        .autoRestart = false,
        .maxRetries = 3
    }
);

// 启动超时计时
timeoutHandler->startTimer();

// 检查是否超时
if (timeoutHandler->isTimedOut()) {
    // 处理超时
}

// 获取剩余时间
int64_t remaining = timeoutHandler->getRemainingTime();

// 停止计时
timeoutHandler->stopTimer();
```

---

### IRetryPolicy

#### 位置
- 接口：`src/interfaces/retry_policy.h`
- 实现：`src/implementations/retry_policy.h`

#### 功能与用处
重试策略接口，支持多种重试策略。

#### 重试策略 (RetryStrategy)

| 策略 | 说明 |
|------|------|
| `FIXED_DELAY` | 固定延迟重试，每次重试间隔相同 |
| `EXPONENTIAL_BACKOFF` | 指数退避重试，延迟时间指数增长 |
| `LINEAR_BACKOFF` | 线性退避重试，延迟时间线性增长 |
| `IMMEDIATE` | 立即重试，无延迟 |

#### 用法
```cpp
#include "implementations/retry_policy.h"

// 创建固定延迟重试策略
auto fixedRetry = RetryPolicyFactory::createFixedDelay(3, 1000);

// 创建指数退避重试策略
auto expRetry = RetryPolicyFactory::createExponentialBackoff(
    3,      // 最大重试次数
    500,     // 初始延迟
    2.0,     // 退避乘数
    10000    // 最大延迟
);

// 创建线性退避重试策略
auto linearRetry = RetryPolicyFactory::createLinearBackoff(
    3,      // 最大重试次数
    500,     // 初始延迟
    1000,    // 延迟增量
    10000    // 最大延迟
);

// 使用重试策略
int attempt = 0;
while (attempt < maxAttempts) {
    try {
        // 执行操作
        executeTask();
        break;
    } catch (const std::exception& e) {
        std::string error = e.what();
        if (retryPolicy->shouldRetry(attempt, error, 0)) {
            attempt++;
            continue;
        }
        throw;
    }
}
```

---

### IMetricsCollector

#### 位置
- 接口：`src/interfaces/metrics_collector.h`
- 实现：`src/implementations/metrics_collector.h`

#### 功能与用处
指标收集器接口，用于收集和统计工作流执行指标。

#### 指标报告类型 (MetricsReportType)

| 类型 | 说明 |
|------|------|
| `SUMMARY` | 摘要报告，显示关键统计信息 |
| `DETAILED` | 详细报告，显示所有执行详情 |
| `JSON` | JSON 格式报告，便于程序处理 |
| `CSV` | CSV 格式报告，便于导入表格工具 |

#### 用法
```cpp
#include "implementations/metrics_collector.h"

// 获取指标收集器（单例）
auto& collector = MetricsCollector::getInstance();

// 记录工作流启动
collector.recordWorkflowStart("MyWorkflow");

// 记录消息处理
collector.recordMessage("MyWorkflow", "MyMessageType");

// 记录工作流完成
collector.recordWorkflowEnd("MyWorkflow", true, "");

// 获取工作流指标
auto metrics = collector.getWorkflowMetrics("MyWorkflow");

// 获取统计信息
auto stats = collector.getStatistics("MyWorkflow");

// 生成报告
std::cout << collector.generateReport(MetricsReportType::SUMMARY);
std::cout << collector.generateWorkflowReport("MyWorkflow", MetricsReportType::JSON);

// 导出到文件
collector.exportMetrics("/tmp/metrics.json", MetricsReportType::JSON);
collector.exportMetrics("/tmp/metrics.csv", MetricsReportType::CSV);

// 清除指标
collector.clearMetrics("MyWorkflow");
```

---

## 快速开始

### 步骤1：创建自定义工作流

```cpp
#include "implementations/workflows/base_workflow.h"

class MyWorkflow : public WorkflowSystem::BaseWorkflow {
public:
    MyWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("MyWorkflow", resourceManager) {}

protected:
    void onStart() override {
        LOG_INFO("[MyWorkflow] Workflow started");
        auto ctx = getContext();
        if (ctx) {
            ctx->set("status", "running");
        }
    }

    void onMessage(const IMessage& message) override {
        LOG_INFO("[MyWorkflow] Received: " + message.getPayload());
    }

    void onInterrupt() override {
        LOG_WARNING("[MyWorkflow] Interrupted");
    }

    void onFinalize() override {
        LOG_INFO("[MyWorkflow] Completed");
    }
};
```

### 步骤2：实现消息接口

```cpp
#include "interfaces/workflow.h"

class MyMessage : public WorkflowSystem::IMessage {
public:
    MyMessage(const std::string& type,
             const std::string& topic,
             const std::string& payload)
        : type_(type), topic_(topic), payload_(payload) {}

    std::string getType() const override { return type_; }
    std::string getTopic() const override { return topic_; }
    std::string getPayload() const override { return payload_; }

private:
    std::string type_;
    std::string topic_;
    std::string payload_;
};
```

### 步骤3：使用工作流系统

```cpp
#include "implementations/system_facade.h"

using namespace WorkflowSystem;

int main() {
    auto& system = SystemFacade::getInstance();
    system.initialize("/tmp/workflow_resources");

    auto manager = system.getManager();

    auto workflow = std::make_shared<MyWorkflow>(
        manager->getResourceManager()
    );
    manager->registerWorkflow("MyWorkflow",
        std::static_pointer_cast<IWorkflow>(workflow));

    manager->startWorkflow("MyWorkflow");

    auto msg = MyMessage("my_type", "my_topic", "my_payload");
    manager->handleMessage(msg);

    manager->getCurrentWorkflow()->finish();

    system.shutdown();
    return 0;
}
```

---

## 设计模式

| 模式 | 用途 | 位置 |
|-------|-------|-------|
| 单例模式 | 全局唯一实例 | `ConsoleLogger`, `SystemFacade`, `MetricsCollector` |
| 策略模式 | 工作流行为多态 | `IWorkflow`, `IMessage`, `ITimeoutHandler`, `IRetryPolicy` |
| 模板方法模式 | 工作流生命周期骨架 | `BaseWorkflow` |
| 观察者模式 | 状态变化通知 | `IWorkflowObserver`, `Observable` |
| 门面模式 | 统一系统接口 | `SystemFacade` |
| 工厂模式 | 创建对象实例 | `TimeoutHandlerFactory`, `RetryPolicyFactory` |

---

## 编译与运行

### 前提条件

- CMake >= 3.10
- C++11 支持的编译器（GCC 4.8+, Clang 3.3+）
- POSIX 线程库（pthread）

### 编译步骤

```bash
cd workflow_system_project
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行示例

```bash
# 基础示例
./build/bin/workflow_example

# 队列示例
./build/bin/queue_example

# 高级功能示例（超时、重试、指标）
./build/bin/advanced_example
```

---

## 已实现的高级功能

### 4. 超时控制 ✓
- 工作流级超时
- 消息级超时
- 超时处理策略（中断、暂停、重试、通知）

### 5. 重试机制 ✓
- 自动重试失败的消息
- 可配置重试次数和间隔
- 多种退避策略（固定延迟、指数退避、线性退避）

### 6. 监控和统计 ✓
- 工作流执行时间统计
- 消息处理吞吐量
- 多格式报告（摘要、详细、JSON、CSV）

---

## 功能规划

### 详细规划文档

完整的功能规划文档请参考: [docs/ROADMAP.md](docs/ROADMAP.md)

该文档包含以下内容：
- 7 个待实现功能的详细设计
- 每个功能的接口定义和数据结构
- 3 个实施阶段的路线图
- 技术架构演进方案
- 依赖库分析建议
- 预估工作量和优先级

### 规划概览

| 功能 | 优先级 | 预估工作量 | 实施阶段 |
|------|----------|-------------|-----------|
| 工作流编排 | P0 | 5-7天 | 第一阶段 |
| 持久化 | P0 | 3-5天 | 第一阶段 |
| 并行执行 | P1 | 5-7天 | 第二阶段 |
| 配置管理 | P1 | 2-3天 | 第二阶段 |
| 事件总线 | P2 | 4-6天 | 第三阶段 |
| 工作流可视化 | P2 | 7-10天 | 第三阶段 |
| 调试工具 | P2 | 3-4天 | 第三阶段 |

---

## 待扩展功能

以下是待规划实施的功能：

### 1. 工作流编排 - [详细设计](docs/ROADMAP.md#31-工作流编排)
- 支持工作流之间的依赖关系
- 工作流链式执行
- 条件分支和循环

### 2. 持久化 - [详细设计](docs/ROADMAP.md#32-持久化)
- 工作流状态持久化到数据库
- 支持断点恢复
- 历史记录查询

### 3. 并行执行 - [详细设计](docs/ROADMAP.md#33-并行执行)
- 多工作流并行执行
- 工作流内任务并行
- 并发控制和同步

### 7. 配置管理 - [详细设计](docs/ROADMAP.md#34-配置管理)
- JSON/YAML 配置文件
- 动态配置加载
- 配置验证

### 8. 事件总线 - [详细设计](docs/ROADMAP.md#35-事件总线)
- 发布-订阅模式
- 跨工作流通信
- 全局事件过滤

### 9. 工作流可视化 - [详细设计](docs/ROADMAP.md#36-工作流可视化)
- 工作流图渲染
- 实时状态显示
- 执行跟踪

### 10. 调试工具 - [详细设计](docs/ROADMAP.md#37-调试工具)
- 断点设置
- 变量检查
- 单步执行

---

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request！
