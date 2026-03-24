# 业务流程管理系统 - 功能概览

## 已实现的核心功能

### 1. 工作流图 (WorkflowGraph)
**接口文件**: `src/interfaces/workflow_graph.h`
**实现文件**: `src/implementations/workflow_graph.h`

- ✅ 节点管理 (IWorkflowNode, WorkflowNode)
  - 节点类型: START, END, TASK, DECISION, MERGE
  - 节点状态: PENDING, RUNNING, COMPLETED, FAILED, SKIPPED
  - 节点前后关系管理
  - 节点属性存储

- ✅ 边管理 (IWorkflowEdge, WorkflowEdge)
  - 有向边，支持条件判断
  - 边权重设置

- ✅ 图管理 (IWorkflowGraph, WorkflowGraph)
  - 拓扑排序
  - DAG 验证
  - 循环检测
  - 起始节点/结束节点查询
  - 图验证器 (IGraphValidator, GraphValidator)

### 2. 工作流编排器 (WorkflowOrchestrator)
**接口文件**: `src/interfaces/workflow_orchestrator.h`
**实现文件**: `src/implementations/workflow_orchestrator.h`

- ✅ 工作流执行
  - 顺序执行 (Sequential)
  - 并行执行 (Parallel)
  - 暂停/恢复支持
  - 终止支持

- ✅ 执行上下文 (IOrchestrationContext, OrchestrationContext)
  - 节点结果存储
  - 变量管理
  - 执行状态跟踪

- ✅ 节点执行器 (INodeExecutor, DefaultNodeExecutor)
  - 默认节点执行器
  - 可自定义执行器

- ✅ 执行策略
  - SEQUENTIAL（顺序）
  - PARALLEL（并行）
  - CONDITIONAL（条件）- 预留

- ✅ 观察者模式集成
  - 添加观察者
  - 移除观察者
  - 清除观察者
  - 工作流事件通知：started, finished, interrupted, error

### 3. 观察者模式 (WorkflowObserver)
**接口文件**: `src/interfaces/workflow_observer.h`
**实现文件**: `src/implementations/workflow_observer_impl.h`

- ✅ 基础观察者
  - BaseWorkflowObserver（空实现）

- ✅ 日志观察者 (LoggingObserver)
  - 将工作流事件记录到日志系统

- ✅ 控制台观察者 (ConsoleObserver)
  - 彩色输出支持
  - 事件类型：🚀 (开始), ✅ (完成), ⏸️ (暂停), ❌ (错误)

- ✅ 统计观察者 (StatisticsObserver)
  - 启动/完成/中断/错误次数统计
  - 执行时间统计
  - 平均耗时计算
  - 实时统计报告生成

- ✅ 回调观察者 (CallbackObserver)
  - 支持自定义回调函数
  - onWorkflowStarted 回调
  - onWorkflowFinished 回调
  - onWorkflowInterrupted 回调
  - onWorkflowError 回调

### 4. 指标收集器 (MetricsCollector)
**接口文件**: `src/interfaces/metrics_collector.h`
**实现文件**: `src/implementations/metrics_collector.h`

- ✅ 工作流指标收集
  - recordWorkflowStart/end
  - recordMessage（消息计数）
  - recordInterrupt/pause/resume

- ✅ 统计功能
  - 单例模式实现
  - 执行次数统计
  - 成功/失败计数
  - 最小/最大/平均耗时
  - 成功率计算

- ✅ 报告生成
  - SUMMARY（摘要）报告
  - DETAILED（详细）报告
  - JSON 格式导出
  - CSV 格式导出

### 5. 重试策略 (RetryPolicy)
**接口文件**: `src/interfaces/retry_policy.h`
**实现文件**: `src/implementations/retry_policy.h`

- ✅ 重试策略
  - FIXED_DELAY（固定延迟）
  - EXPONENTIAL_BACKOFF（指数退避）
  - LINEAR_BACKOFF（线性退避）
  - IMMEDIATE（立即重试）

- ✅ 重试条件
  - ON_ERROR（错误时）
  - ON_TIMEOUT（超时时）
  - ON_EXCEPTION（异常时）
  - ON_FAILURE（失败时）
  - CUSTOM（自定义条件）

- ✅ 重试管理
  - shouldRetry 判断
  - getNextDelay 延迟计算
  - 最大重试次数限制
  - 最大延迟限制
  - 重试回调支持
  - 工厂方法：createFixedDelay, createExponentialBackoff, createLinearBackoff, createImmediate, createCustom

### 6. 超时处理器 (TimeoutHandler)
**接口文件**: `src/interfaces/timeout_handler.h`
**实现文件**: `src/implementations/timeout_handler.h`

- ✅ 超时管理
  - setTimeout 设置
  - startTimer 启动
  - stopTimer 停止
  - resetTimer 重置
  - isTimedOut 判断
  - getElapsedTime 获取已用时间
  - getRemainingTime 获取剩余时间

- ✅ 超时策略
  - INTERRUPT（中断）
  - PAUSE（暂停）
  - RETRY（重试）
  - NOTIFY_ONLY（仅通知）

- ✅ 线程安全
  - std::mutex 保护
  - std::thread 监控线程
  - std::condition_variable 同步

### 7. 资源管理器 (ResourceManager)
**接口文件**: `src/interfaces/resource_manager.h`
**实现文件**: `src/implementations/filesystem_resource_manager.h`

- ✅ 文件系统资源管理
  - createDirectory 创建目录
  - getResource 获取资源
  - cleanup 清理资源
  - cleanupAll 清理所有
  - getAllResources 列出所有

- ✅ 资源类型
  - DIRECTORY（目录）
  - FILE（文件）

- ✅ 工具类 (FsUtils)
  - createDirectories 递归创建目录
  - removeDirectory 递归删除目录

### 8. 配置管理器 (ConfigManager)
**接口文件**: `src/interfaces/config_manager.h`
**实现文件**: `src/implementations/json_config_manager.h`

- ✅ JSON 配置管理
  - 加载配置文件
  - 保存配置文件
  - 获取配置值
  - 设置配置值
  - 验证配置格式

### 9. 工作流持久化 (WorkflowPersistence)
**接口文件**: `src/interfaces/workflow_persistence.h`
**实现文件**: `src/implementations/sqlite_workflow_persistence.h`

- ✅ SQLite 数据库支持
  - 嵌入式 SQLite3 数据库
  - 事务支持
  - 参数化查询（防止 SQL 注入）
  - 线程安全（互斥锁保护）

- ✅ 工作流记录管理
  - 保存工作流记录（saveWorkflow）
  - 根据 ID 获取工作流（getWorkflowById）
  - 获取所有工作流（getAllWorkflows）
  - 根据名称查询（模糊匹配）（getWorkflowsByName）
  - 根据状态查询（getWorkflowsByState）
  - 根据时间范围查询（getWorkflowsByTimeRange）
  - 删除工作流（deleteWorkflow）
  - 清空所有工作流（clearAllWorkflows）

- ✅ 统计功能
  - 获取总工作流数（getTotalWorkflowCount）
  - 获取成功的工作流数量（getSuccessCount）
  - 获取失败的工作流数量（getFailureCount）
  - 计算成功率（getSuccessRate）
  - 获取平均执行时间（getAverageDuration）

- ✅ 工作流定义管理
  - 保存工作流定义（JSON 格式）（saveWorkflowDefinition）
  - 获取工作流定义（getWorkflowDefinition）
  - 获取所有工作流定义（getAllWorkflowDefinitions）

- ✅ 数据库表结构
  - `workflows` 表：工作流执行记录
    - id (TEXT PRIMARY KEY)
    - workflow_name (TEXT)
    - state (INTEGER)
    - start_time (INTEGER)
    - end_time (INTEGER)
    - duration (INTEGER)
    - success (INTEGER)
    - error_message (TEXT)
    - retry_count (INTEGER)
    - input_config (TEXT)
  - `workflow_definitions` 表：工作流定义存储
    - workflow_id (TEXT PRIMARY KEY)
    - definition_json (TEXT)
    - created_at (INTEGER)

- ✅ 索引优化
  - workflow_name 索引
  - state 索引
  - start_time 索引

- ✅ 单例模式
  - 全局唯一的持久化管理器实例

### 10. 工作流管理器 (WorkflowManager)
**接口文件**: `src/interfaces/workflow_manager.h`
**实现文件**: `src/implementations/workflow_manager.h`

- ✅ 工作流管理
  - 创建工作流
  - 删除工作流
  - 获取工作流
  - 列出所有工作流
  - 工作流执行控制

### 11. 工作流版本控制 (WorkflowVersionControl)
**接口文件**: `src/interfaces/workflow_version_control.h`
**实现文件**: `src/implementations/workflow_version_control_impl.h`
**示例程序**: `src/version_control_example.cpp`

- ✅ 版本管理
  - saveVersion 保存版本
  - getAllVersions 获取所有版本
  - getLatestVersion 获取最新版本
  - getVersionDefinition 获取指定版本定义
  - rollbackToVersion 回滚到指定版本
  - compareVersions 版本比较
  - setActiveVersion 设置激活版本
  - deleteVersion 删除指定版本
  - deleteAllVersions 删除所有版本
  - getAllWorkflowNames 获取所有工作流名称

- ✅ 版本元数据
  - versionId 版本ID
  - workflowName 工作流名称
  - definition 工作流定义
  - author 作者
  - description 描述
  - createdAt 创建时间
  - parentVersion 父版本
  - isActive 是否激活

- ✅ 版本差异
  - versionFrom 起始版本
  - versionTo 目标版本
  - summary 差异摘要

- ✅ 与持久化集成
  - 使用 IWorkflowPersistence 存储版本信息
  - 支持 JSON 格式存储版本数据

### 12. 条件分支工作流 (ConditionalWorkflow)
**接口文件**: `src/interfaces/conditional_workflow.h`
**实现文件**: `src/implementations/conditional_workflow_impl.h`
**示例程序**: `src/conditional_example.cpp`

- ✅ 分支结构 (Branch)
  - name 分支名称
  - conditionName 条件名称
  - condition 条件判断函数 (std::function<bool()>)
  - workflow 分支工作流
  - priority 优先级
  - description 分支描述

- ✅ 条件评估器 (ConditionEvaluator)
  - evaluate 评估条件
  - registerCondition 注册条件
  - removeCondition 移除条件

- ✅ 条件工作流 (ConditionalWorkflow)
  - addBranch 添加分支
  - setDefaultBranch 设置默认分支
  - getSelectedBranch 获取选中的分支
  - getAllBranches 获取所有分支
  - getDefaultBranch 获取默认分支
  - clearBranches 清空所有分支
  - evaluateAndExecute 评估并执行

- ✅ IWorkflow 接口实现
  - start 启动工作流
  - handleMessage 处理消息
  - execute 执行工作流
  - interrupt 中断工作流
  - cancel 取消工作流
  - finish 完成工作流
  - error 错误处理

- ✅ 功能特性
  - 基于条件动态选择执行分支
  - 支持多优先级分支选择
  - 支持默认分支 fallback
  - 上下文数据在工作流间传递
  - 线程安全的执行
  - 观察者模式集成

### 13. 工作流模板 (WorkflowTemplate)
**接口文件**: `src/interfaces/workflow_template.h`
**实现文件**: `src/implementations/workflow_template_impl.h`
**示例程序**: `src/workflow_template_example.cpp`

- ✅ 模板参数 (TemplateParameter)
  - name 参数名称
  - type 参数类型（string, int, bool, etc.）
  - defaultValue 默认值
  - required 是否必填
  - description 参数描述

- ✅ 工作流模板接口 (IWorkflowTemplate)
  - getTemplateId 获取模板ID
  - getTemplateName 获取模板名称
  - getDescription 获取模板描述
  - getVersion 获取模板版本
  - getParameters 获取所有参数定义
  - validateParameters 验证参数有效性
  - createWorkflow 根据参数创建工作流
  - createGraph 根据参数创建工作流图

- ✅ 抽象模板基类 (AbstractWorkflowTemplate)
  - 提供模板元数据的通用实现
  - 提供参数验证的通用实现
  - 支持类型检查（int, string, bool）
  - 支持必填参数验证
  - 提供 addParameter 和 getParameter 辅助方法

- ✅ 模板注册表 (WorkflowTemplateRegistry)
  - registerTemplate 注册模板
  - unregisterTemplate 注销模板
  - hasTemplate 检查模板是否存在
  - listTemplates 列出所有模板ID
  - getTemplate 获取指定模板
  - createWorkflowFromTemplate 从模板创建工作流
  - createGraphFromTemplate 从模板创建工作流图

- ✅ 模板参数构建器 (TemplateParameterBuilder)
  - addParameter 添加参数（支持两种形式）
  - build 构建参数映射
  - reset 重置构建器
  - 支持链式调用

- ✅ 设计模式
  - 原型模式：通过模板创建具体工作流
  - 建造者模式：逐步构建工作流模板
  - 注册表模式：管理可用的工作流模板

- ✅ 功能特性
  - 可重用的工作流结构定义
  - 参数化模板实例化
  - 参数类型验证
  - 必填参数检查
  - 默认值支持
  - 模板版本管理
  - 模板元数据管理

### 14. 系统门面 (SystemFacade)
**接口文件**: `src/interfaces/workflow.h`
**实现文件**: `src/implementations/system_facade.h`

- ✅ 系统门面
  - 统一接口访问所有系统功能
  - 简化使用复杂度

### 核心工具类

- ✅ 日志系统 (Logger)
  - 多级日志：INFO, WARNING, ERROR
  - 控制台输出

- ✅ 时间工具 (TimeUtils)
  - 获取当前时间戳（毫秒）

- ✅ 类型系统 (Types)
  - Any 类型包装
  - 统一类型定义

## 已实现的示例程序

### 基础示例
1. **workflow_example** - 基本工作流示例
   - 节点创建
   - 边添加
   - 执行控制

2. **queue_example** - 队列示例
   - 线程安全
   - 环形缓冲队列

3. **advanced_example** - 高级特性示例
   - 重试机制
   - 资源管理

4. **config_example** - 配置驱动示例
   - JSON 配置加载
   - 动态工作流创建

5. **orchestration_example** - 编排器完整示例
   - 顺序执行
   - 并行执行
   - 错误处理
   - 暂停/恢复
   - 配置驱动

6. **observer_example** - 观察者模式完整演示
   - ConsoleObserver 演示
   - LoggingObserver 演示
   - StatisticsObserver 演示（多次执行统计）
   - CallbackObserver 演示
   - 组合多个观察者
   - 错误处理演示
   - 并行执行与观察者
   - 动态管理观察者
   - 进度回调与观察者
   - 自定义观察者实现

7. **persistence_example** - SQLite 持久化功能示例
   - 数据库初始化和关闭
   - 保存和获取工作流记录
   - 查询工作流（按 ID、名称、状态、时间范围）
   - 统计信息（总数、成功率、平均耗时）
   - 保存和获取工作流定义
   - 删除和清空工作流记录

8. **comprehensive_example** - 综合示例
   - 完整的工作流编排演示
   - 配置文件驱动
   - 多种观察者组合
   - 错误处理和重试机制

9. **version_control_example** - 版本控制示例
   - 版本保存和回滚
   - 版本元数据管理
   - 版本差异比较

10. **conditional_example** - 条件分支工作流示例
   - 根据条件动态选择执行分支
   - 支持多优先级分支
   - 支持默认分支 fallback
   - 上下文数据在工作流间传递

11. **workflow_template_example** - 工作流模板示例
   - 定义工作流模板
   - 注册模板到注册表
   - 从模板创建工作流
   - 参数化模板实例化
   - 参数验证
   - 使用参数构建器

## 设计模式应用

✅ **观察者模式** (Observer)
- 多个观察者监听同一工作流
- 支持动态添加/移除观察者

✅ **策略模式** (Strategy)
- 执行策略：Sequential, Parallel
- 重试策略：Fixed Delay, Exponential Backoff, Linear Backoff
- 超时策略：Interrupt, Pause, Retry, Notify

✅ **单例模式** (Singleton)
- MetricsCollector 使用单例模式

✅ **工厂模式** (Factory)
- RetryPolicyFactory
- TimeoutHandlerFactory

✅ **门面模式** (Facade)
- SystemFacade 提供统一接口

## 线程安全

- ✅ std::mutex 保护共享数据
- ✅ std::atomic 用于线程安全标志
- ✅ std::condition_variable 用于线程同步
- ✅ std::thread 用于后台监控

## 编译和构建

✅ **CMake 配置**
- 支持 C++11 标准
- 多线程支持 (Threads::Threads)
- 生成的可执行文件位于 `build/bin/`

## 技术栈

- **语言**: C++11
- **构建工具**: CMake
- **标准库**: STL (string, map, vector, thread, mutex, etc.)
- **头文件库**: Header-only 库模式

## 项目结构

```
workflow_system_project/
├── CMakeLists.txt
├── src/
│   ├── core/              # 核心工具类
│   ├── interfaces/        # 接口定义
│   ├── implementations/  # 具体实现
│   └── workflows/        # 预置工作流
└── build/               # 编译输出目录
```

### 15. 工作流调度器 (WorkflowScheduler)
**接口文件**: `src/interfaces/workflow_scheduler.h`
**实现文件**: `src/implementations/workflow_scheduler_impl.h`

- ✅ 调度配置 (ScheduleConfig)
  - scheduleId 调度ID
  - workflowName 工作流名称
  - type 调度类型 (ONCE, INTERVAL, DAILY, WEEKLY, MONTHLY, CRON)
  - intervalMs 间隔时间
  - cronExpression Cron表达式
  - startTime/endTime 时间范围
  - maxExecutions 最大执行次数

- ✅ 调度类型 (ScheduleType)
  - ONCE 执行一次
  - INTERVAL 固定间隔
  - DAILY 每天执行
  - WEEKLY 每周执行
  - MONTHLY 每月执行
  - CRON Cron表达式

- ✅ 调度管理
  - schedule 添加调度任务
  - cancelSchedule 取消调度
  - pauseSchedule/resumeSchedule 暂停/恢复调度
  - updateSchedule 更新调度配置

- ✅ 调度器控制
  - start/stop 启动/停止调度器
  - isRunning 检查运行状态
  - setExecutionCallback 设置执行回调

- ✅ 查询接口
  - getNextExecutionTime 获取下次执行时间
  - getAllNextExecutionTimes 获取所有下次执行时间
  - getScheduleRecords 获取调度记录

### 16. 工作流追踪器 (WorkflowTracer)
**接口文件**: `src/interfaces/workflow_tracer.h`
**实现文件**: `src/implementations/workflow_tracer_impl.h`

- ✅ 追踪事件类型 (TraceEventType)
  - WORKFLOW_STARTED/COMPLETED/FAILED
  - NODE_STARTED/COMPLETED/FAILED
  - MESSAGE_RECEIVED, STATE_CHANGED
  - ERROR_OCCURRED, RETRY_ATTEMPT, TIMEOUT_OCCURRED
  - RESOURCE_CREATED/RELEASED

- ✅ 追踪记录
  - TraceEvent 追踪事件
  - ExecutionTrace 执行追踪
  - TraceStatistics 追踪统计

- ✅ 追踪接口
  - startTrace/endTrace 开始/结束追踪
  - recordEvent 记录事件
  - recordStateChange 记录状态变更
  - recordError 记录错误
  - recordNodeExecution 记录节点执行

- ✅ 查询接口
  - getTrace 获取追踪信息
  - getTracesByWorkflow 按工作流获取追踪
  - getActiveTraces 获取活跃追踪
  - getStatistics 获取统计信息

- ✅ 管理接口
  - clearTraces 清除追踪历史
  - setMaxTraces 设置最大追踪数量
  - exportTraces 导出追踪数据 (JSON/CSV)

### 17. 告警管理器 (AlertManager)
**接口文件**: `src/interfaces/alert_manager.h`

- ✅ 告警级别 (AlertLevel)
  - INFO 信息
  - WARNING 警告
  - ERROR 错误
  - CRITICAL 严重

- ✅ 告警类型 (AlertType)
  - WORKFLOW_FAILED 工作流失败
  - WORKFLOW_TIMEOUT 工作流超时
  - WORKFLOW_STUCK 工作流卡住
  - RESOURCE_EXHAUSTED 资源耗尽
  - HIGH_ERROR_RATE 高错误率
  - SCHEDULE_MISSED 调度错过
  - SYSTEM_OVERLOAD 系统过载
  - CUSTOM 自定义

- ✅ 告警规则管理
  - addAlertRule 添加告警规则
  - removeAlertRule 移除告警规则
  - updateAlertRule 更新告警规则
  - enableAlertRule 启用/禁用规则

- ✅ 告警触发
  - triggerAlert 触发告警
  - resolveAlert 解决告警
  - acknowledgeAlert 确认告警

- ✅ 通知管理
  - setAlertCallback 设置告警回调
  - addNotificationChannel 添加通知渠道
  - removeNotificationChannel 移除通知渠道

### 18. 死信队列 (DeadLetterQueue)
**接口文件**: `src/interfaces/dead_letter_queue.h`

- ✅ 死信原因 (DeadLetterReason)
  - MAX_RETRIES_EXCEEDED 超过最大重试次数
  - TIMEOUT 超时
  - VALIDATION_ERROR 验证错误
  - EXECUTION_ERROR 执行错误
  - RESOURCE_UNAVAILABLE 资源不可用
  - DEPENDENCY_FAILED 依赖失败
  - CANCELLED 已取消

- ✅ 死信状态 (DeadLetterStatus)
  - PENDING 等待处理
  - RETRYING 重试中
  - RESOLVED 已解决
  - DISCARDED 已丢弃
  - MANUAL_REVIEW 人工审核中

- ✅ 死信项 (DeadLetterItem)
  - 保存失败任务的完整上下文
  - 支持重试计数和最大重试次数
  - 记录错误消息和堆栈跟踪

- ✅ 队列操作
  - enqueue 入队
  - enqueueFromError 从错误创建
  - getItem 获取死信项
  - getPendingItems 获取待处理项
  - getItemsByStatus 按状态获取项

- ✅ 重试和解决
  - retry 重试
  - resolve 解决
  - discard 丢弃
  - moveToManualReview 移至人工审核

## 可能的改进方向

以下是一些可以考虑继续完善的方向：

1. **分布式工作流** - 支持跨机器工作流执行
2. **工作流可视化** - 工作流图可视化工具
3. **性能监控仪表板** - 实时监控和统计展示
4. **工作流调试器** - 断点调试、单步执行
5. **更多资源类型** - 内存资源管理、网络资源管理
6. **工作流 DSL** - 领域特定语言支持
