# 功能特性速查

快速了解每个模块的核心能力。

| 模块 | 接口 | 实现 | 能力 |
|------|------|------|------|
| 工作流 | `IWorkflow` | `BaseWorkflow` | 生命周期、异步执行、消息驱动 |
| 上下文 | `IWorkflowContext` | `WorkflowContext` | 键值存储、线程安全、对象存储 |
| 编排 | `IWorkflowOrchestrator` | `WorkflowOrchestrator` | DAG编排、并行执行、循环检测 |
| 管理 | `IWorkflowManager` | `WorkflowManager` | 注册、启动、停止、清理 |
| 持久化 | `IWorkflowPersistence` | `SqliteWorkflowPersistence` | SQLite存储、历史记录、统计 |
| 调度 | `IWorkflowScheduler` | `WorkflowSchedulerImpl` | 定时、周期、Cron |
| 图 | `IWorkflowGraph` | `WorkflowGraph` | 节点/边管理、拓扑排序 |
| 模板 | `IWorkflowTemplate` | `WorkflowTemplateImpl` | 参数化、实例化 |
| 版本 | `IWorkflowVersionControl` | `WorkflowVersionControlImpl` | 版本创建、回滚 |
| 追踪 | `IWorkflowTracer` | `WorkflowTracerImpl` | 事件记录、统计 |
| 观察者 | `IWorkflowObserver` | `WorkflowObserverImpl` | 事件回调 |
| 条件 | `IConditionalWorkflow` | `ConditionalWorkflowImpl` | 分支判断 |
| 配置 | `IConfigManager` | `JsonConfigManager` | JSON读写、类型安全 |
| 检查点 | `ICheckpointManager` | `CheckpointManagerImpl` | 状态快照、恢复 |
| 告警 | `IAlertManager` | `AlertManagerImpl` | 规则引擎、状态管理 |
| 死信 | `IDeadLetterQueue` | `DeadLetterQueueImpl` | 失败处理、重试 |
| 指标 | `IMetricsCollector` | `MetricsCollector` | 统计、报告 |
| 重试 | `IRetryPolicy` | `RetryPolicy` | 固定/指数退避 |
| 超时 | `ITimeoutHandler` | `TimeoutHandler` | 超时检测 |
| 资源 | `IResourceManager` | `FilesystemResourceManager` | 文件管理 |
| 熔断 | — | `CircuitBreaker` | 三态熔断 |
| 内存池 | — | `MemoryPool<T>` | 高性能分配 |
| 对象池 | — | `ObjectPool<T>` | RAII自动管理 |
| 异步日志 | — | `AsyncLogger` | 后台写入 |
| 系统门面 | — | `SystemFacade` | 统一入口 |
