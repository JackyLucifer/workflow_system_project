# 插件系统集成指南

## 概述

WorkflowSystem 包含一个独立的插件子系统，位于 `plugins/` 目录。支持动态加载、依赖解析、服务注册和消息通信。

## 插件接口

```cpp
// plugins/include/plugin_interface.h
class IPlugin {
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual bool initialize(IPluginContext* ctx) = 0;
    virtual void shutdown() = 0;
};
```

## 创建插件

参考 `plugins/examples/hello_plugin/hello_plugin.cpp`：

1. 继承 `IPlugin` 接口
2. 实现 `initialize()` 和 `shutdown()`
3. 导出插件符号

## 构建插件

插件子系统有独立的 `plugins/CMakeLists.txt`：

```bash
cd plugins
mkdir build && cd build
cmake ..
make
```

## 示例

- `hello_plugin` — 最简插件示例
- `advanced_plugin` — 带依赖和服务注册的插件

详见 `plugins/README.md`。
