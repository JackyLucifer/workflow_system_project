# WorkflowSystem — C++ 工作流引擎

一个功能完整的 C++ 工作流管理系统，支持工作流编排、调度、持久化、版本控制等企业级特性。

> **C++14** · **线程安全** · **头文件+库** 架构 · **SQLite 持久化** · **插件系统**

---

## 快速开始

### 环境要求

- GCC 5+ / Clang 5+（支持 C++14）
- CMake 3.10+
- SQLite3 开发库
- pthread

### 编译

```bash
cd workflow_system_project
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 运行测试

```bash
# 运行所有测试
make test

# 或单独运行
./bin/test_all
./bin/test_core_types
# ...
```

### 运行示例

```bash
./bin/workflow_example        # 基础工作流
./bin/config_example          # 配置管理
./bin/orchestration_example   # 工作流编排
./bin/persistence_example     # 数据持久化
./bin/scheduler_example       # 任务调度
./bin/observer_example        # 观察者模式
./bin/advanced_example        # 高级功能
./bin/comprehensive_example   # 综合示例
```

---

## 项目结构

```
workflow_system_project/
├── include/workflow_system/       # 公共头文件
│   ├── core/                      # 核心工具（Any、日志、类型、异常）
│   ├── interfaces/                # 纯虚接口定义
│   └── implementations/           # 实现类头文件
├── src/implementations/           # 实现类 .cpp 文件
├── examples/                      # 示例程序
├── tests/                         # 单元测试
├── plugins/                       # 插件系统（独立子项目）
│   ├── include/                   # 插件接口
│   ├── src/                       # 插件加载器实现
│   └── examples/                  # 插件示例
└── CMakeLists.txt                 # 构建配置
```

---

## 核心模块

### 接口层（`interfaces/`）

所有核心功能通过纯虚接口定义，便于测试和替换实现：

| 接口 | 说明 |
|------|------|
| `IWorkflow` | 工作流生命周期（启动、消息、中断、取消） |
| `IWorkflowContext` | 工作流上下文数据传递 |
| `IWorkflowObserver` | 工作流事件观察者 |
| `IWorkflowManager` | 工作流注册与管理 |
| `IWorkflowOrchestrator` | 多工作流编排（DAG、并行、串行） |
| `IWorkflowPersistence` | 工作流状态持久化 |
| `IWorkflowScheduler` | 定时/周期任务调度 |
| `IWorkflowGraph` | 工作流图（节点、边、拓扑排序） |
| `IWorkflowTemplate` | 工作流模板与参数化 |
| `IWorkflowVersionControl` | 版本管理与回滚 |
| `IWorkflowTracer` | 执行追踪与事件记录 |
| `IConfigManager` | JSON 配置管理 |
| `IRetryPolicy` | 重试策略（固定/指数退避） |
| `ITimeoutHandler` | 超时处理 |
| `ICheckpointManager` | 检查点管理 |
| `IAlertManager` | 告警规则与通知 |
| `IDeadLetterQueue` | 死信队列 |
| `IMetricsCollector` | 指标收集与统计 |
| `IResourceManager` | 资源管理 |
| `IConditionalWorkflow` | 条件分支工作流 |

### 核心工具（`core/`）

| 工具 | 说明 |
|------|------|
| `Any` | 类型擦除容器（类似 `std::any`） |
| `Logger` | 单例日志系统，支持多级别 |
| `IdGenerator` | 唯一 ID 生成器 |
| `TimeUtils` | 时间格式化工具 |
| `StringUtils` | 字符串工具函数 |
| `Exceptions` | 自定义异常类型 |

### 实现层（`implementations/`）

| 实现 | 说明 |
|------|------|
| `WorkflowOrchestrator` | DAG 编排，并行执行，循环检测 |
| `WorkflowManager` | 工作流注册、启动、停止、清理 |
| `WorkflowGraph` | 图结构，拓扑排序，DAG 检测 |
| `SqliteWorkflowPersistence` | SQLite 持久化存储 |
| `JsonConfigManager` | JSON 配置文件读写 |
| `WorkflowSchedulerImpl` | 定时/周期/Cron 调度 |
| `WorkflowTemplateImpl` | 模板参数化与实例化 |
| `WorkflowVersionControlImpl` | 版本创建、回滚、对比 |
| `WorkflowTracerImpl` | 执行事件追踪与统计 |
| `CheckpointManagerImpl` | 内存检查点（文件持久化） |
| `CircuitBreaker` | 熔断器（Closed/Open/HalfOpen） |
| `RetryPolicy` | 固定延迟/指数退避重试 |
| `TimeoutHandler` | 超时检测与回调 |
| `AlertManagerImpl` | 告警规则引擎 |
| `DeadLetterQueueImpl` | 死信队列（入队/重试/清理） |
| `MetricsCollector` | 指标收集与统计报告 |
| `MemoryPool<T>` | 高性能固定大小内存池 |
| `ObjectPool<T>` | RAII 对象池（智能指针自动归还） |
| `AsyncLogger` | 异步日志（后台线程写入） |
| `SystemFacade` | 系统门面（统一接口） |
| `BaseWorkflow` | 工作流基类（模板方法模式） |
| `ConditionalWorkflowImpl` | 条件分支工作流 |
| `FilesystemResourceManager` | 文件系统资源管理 |
| `WorkflowContext` | 上下文实现（线程安全） |

---

## 使用示例

### 1. 创建并运行工作流

```cpp
#include "workflow_system/interfaces/workflow.h"
#include "workflow_system/implementations/workflow_context.h"

// 继承 BaseWorkflow 或实现 IWorkflow 接口
class MyWorkflow : public BaseWorkflow {
public:
    MyWorkflow() : BaseWorkflow("MyWorkflow") {}
    
    void handleMessage(const IMessage& msg) override {
        // 处理消息
    }
    
    void run() override {
        // 工作流主体逻辑
    }
};

// 使用
auto workflow = std::make_shared<MyWorkflow>();
auto context = std::make_shared<WorkflowContext>();
workflow->setContext(context);
auto future = workflow->execute();
```

### 2. 工作流编排（DAG）

```cpp
#include "workflow_system/implementations/workflow_orchestrator.h"

WorkflowOrchestrator orchestrator;
orchestrator.addNode("fetch_data");
orchestrator.addNode("process");
orchestrator.addNode("save");
orchestrator.addEdge("fetch_data", "process");
orchestrator.addEdge("process", "save");
orchestrator.execute();
```

### 3. 配置管理

```cpp
#include "workflow_system/implementations/json_config_manager.h"

JsonConfigManager config("config.json");
config.load();
std::string host = config.getString("database.host", "localhost");
int port = config.getInt("database.port", 3306);
```

### 4. 持久化存储

```cpp
#include "workflow_system/implementations/sqlite_workflow_persistence.h"

SqliteWorkflowPersistence persistence("workflows.db");
persistence.initialize();
persistence.saveWorkflow(record);
auto record = persistence.loadWorkflow("workflow_123");
```

### 5. 内存池

```cpp
#include "workflow_system/implementations/memory_pool.h"

MemoryPool<MyData, 1024> pool;

MyData* data = pool.allocate();
new(data) MyData(42);   // placement new
data->~MyData();         // 手动析构
pool.deallocate(data);   // 归还

// 或使用 RAII 对象池
ObjectPool<MyData> objPool(10);
{
    auto obj = objPool.acquire();
    obj->value = 42;
} // 自动归还
```

### 6. 观察者模式

```cpp
#include "workflow_system/implementations/workflow_observer_impl.h"

auto observer = std::make_shared<WorkflowObserverImpl>();
observer->setOnStarted([](const std::string& name) {
    std::cout << "Workflow started: " << name << std::endl;
});
workflow->addObserver(observer);
```

### 7. 熔断器

```cpp
#include "workflow_system/implementations/circuit_breaker.h"

CircuitBreaker breaker(5, 1000); // 5次失败后熔断，1秒后尝试恢复
if (breaker.allowRequest()) {
    // 执行操作
    breaker.recordSuccess();
} else {
    // 被熔断，走降级逻辑
}
```

---

## 插件系统

项目包含独立的插件子系统（`plugins/`），支持：

- 动态插件加载与卸载
- 插件依赖解析
- 服务注册与发现
- 消息总线通信
- 版本管理

详见 `plugins/README.md`。

---

## 测试

项目包含 **15 个测试套件，240+ 个测试用例**：

| 测试文件 | 覆盖模块 |
|----------|----------|
| `test_core_types` | Any、IdGenerator、TimeUtils、StringUtils |
| `test_all` | WorkflowContext、Logger、CircuitBreaker、Checkpoint |
| `test_workflow_graph` | WorkflowNode、Edge、Graph、拓扑排序 |
| `test_config_manager` | JsonConfigManager |
| `test_extended_modules` | RetryPolicy、Timeout、Alert、DeadLetter、Metrics、Scheduler、Tracer |
| `test_workflow_orchestrator` | DAG编排、并行执行、循环检测 |
| `test_workflow_persistence` | SQLite持久化 |
| `test_workflow_version_control` | 版本创建、回滚 |
| `test_workflow_template` | 模板参数化 |
| `test_conditional_workflow` | 条件分支 |
| `test_workflow_observer` | 观察者通知 |
| `test_workflow_manager` | 工作流生命周期管理 |
| `test_memory_pool` | MemoryPool、ObjectPool、线程安全 |
| `test_async_logger` | 异步日志 |
| `test_system_facade` | 系统门面 |

---

## 设计模式

本项目运用了多种经典设计模式：

| 模式 | 应用 |
|------|------|
| **策略模式** | 不同工作流实现不同行为策略 |
| **观察者模式** | 工作流事件通知（WorkflowObserver） |
| **模板方法** | BaseWorkflow 定义算法骨架 |
| **组合模式** | IWorkflow 持有观察者列表 |
| **单例模式** | Logger、MetricsCollector |
| **对象池模式** | MemoryPool、ObjectPool |
| **熔断器模式** | CircuitBreaker |
| **死信队列模式** | DeadLetterQueue |
| **门面模式** | SystemFacade |

---

## 依赖

| 依赖 | 用途 | 必须 |
|------|------|------|
| SQLite3 | 工作流持久化 | 是 |
| pthread | 线程安全 | 是 |
| C++14 | 语言标准 | 是 |

---

## 许可

私有项目，保留所有权利。
