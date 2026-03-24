# WorkflowSystem 插件系统

一个完整的 C++11 插件框架，支持动态加载、依赖注入、依赖解析、消息总线、配置管理和版本控制。

## 特性

- **动态加载**: 运行时加载/卸载 `.so` 插件
- **依赖注入**: 宿主向插件注入服务对象
- **依赖解析**: 自动计算插件加载顺序（拓扑排序）
- **消息总线**: 发布/订阅模式的插件间通信
- **配置管理**: JSON 格式的配置持久化
- **资源管理**: 追踪和清理插件资源
- **版本控制**: SemVer 语义版本兼容性检查

## 目录结构

```
plugins/
├── include/              # 头文件（header-only）
│   ├── plugin_interface.h      # 插件接口定义
│   ├── plugin_loader.h         # 基础加载器
│   ├── service_registry.h      # 服务注册中心
│   ├── dependency_resolver.h   # 依赖解析器
│   ├── message_bus.h           # 消息总线
│   ├── plugin_config.h         # 配置管理
│   ├── resource_manager.h      # 资源管理
│   ├── advanced_plugin_loader.h # 高级加载器
│   └── version_manager.h       # 版本管理
├── src/
│   └── plugin_loader.cpp       # 加载器实现
├── examples/             # 示例
│   ├── hello_plugin/           # 基础插件示例
│   ├── advanced_plugin/        # 高级功能示例
│   └── test_plugin_host.cpp    # 集成测试
├── tests/
│   └── plugin_tests.cpp        # 单元测试
└── CMakeLists.txt
```

## 编译

```bash
cd plugins
mkdir build && cd build
cmake ..
make -j4
```

编译产物：
- `build/plugins/libplugin_core.so` - 核心库
- `build/plugins/hello_plugin.so` - 示例插件
- `build/plugins/advanced_plugin.so` - 高级示例
- `build/bin/test_plugin_host` - 集成测试程序
- `build/bin/plugin_tests` - 单元测试程序

## 快速开始

### 1. 创建插件

```cpp
// my_plugin.cpp
#include "plugin_interface.h"
#include <iostream>

class MyPlugin : public WorkflowSystem::Plugin::IPlugin {
public:
    void initialize(WorkflowSystem::Plugin::IPluginContext* ctx) override {
        std::cout << "MyPlugin 初始化" << std::endl;
    }
    
    void shutdown() override {
        std::cout << "MyPlugin 关闭" << std::endl;
    }
    
    std::string getName() const override {
        return "MyPlugin";
    }
    
    std::string getVersion() const override {
        return "1.0.0";
    }
    
    WorkflowSystem::Plugin::PluginResult execute(
        const WorkflowSystem::Plugin::PluginParams& params) override {
        return WorkflowSystem::Plugin::PluginResult::ok("执行成功");
    }
};

REGISTER_PLUGIN(MyPlugin)
```

### 2. 编译插件

```cmake
add_library(my_plugin SHARED my_plugin.cpp)
target_include_directories(my_plugin PRIVATE ${PLUGIN_INCLUDE_DIR})
set_target_properties(my_plugin PROPERTIES PREFIX "")
```

### 3. 宿主程序

```cpp
#include "advanced_plugin_loader.h"

int main() {
    using namespace WorkflowSystem::Plugin;
    
    // 获取高级加载器
    AdvancedPluginLoader& loader = AdvancedPluginLoader::getInstance();
    
    // 注册服务供插件使用
    ServiceRegistry::getInstance().registerInstance("logger", &myLogger);
    
    // 加载插件
    loader.loadPlugin("./my_plugin.so");
    
    // 执行插件
    auto plugins = loader.getLoadedPlugins();
    for (auto& plugin : plugins) {
        plugin->execute(PluginParams());
    }
    
    // 卸载
    loader.unloadAllPlugins();
    return 0;
}
```

## 核心组件

### 服务注册中心 (ServiceRegistry)

```cpp
// 注册服务
int myService = 42;
ServiceRegistry::getInstance().registerInstance("my_service", &myService);

// 在插件中获取服务
auto* service = context->getService<int>("my_service");
```

### 依赖解析 (DependencyResolver)

```cpp
DependencyResolver resolver;
resolver.registerNode("plugin_a", {"plugin_b", "plugin_c"});
resolver.registerNode("plugin_b", {});
resolver.registerNode("plugin_c", {"plugin_b"});

auto result = resolver.resolve();
// result.loadOrder = ["plugin_b", "plugin_c", "plugin_a"]
```

### 消息总线 (MessageBus)

```cpp
MessageBus& bus = MessageBus::getInstance();

// 订阅
uint64_t subId = bus.subscribe("topic", [](const Message& msg) {
    std::cout << "收到: " << msg.data << std::endl;
});

// 发布
bus.publish("topic", "Hello World");

// 取消订阅
bus.unsubscribe(subId);
```

### 配置管理 (PluginConfigManager)

```cpp
PluginConfigManager& config = PluginConfigManager::getInstance();

// 设置配置
config.setGlobal("timeout", ConfigValue::fromInt(5000));
config.setPluginConfig("my_plugin", "enabled", ConfigValue::fromBool(true));

// 读取配置
int timeout = config.getGlobalInt("timeout", 3000);
bool enabled = config.getPluginBool("my_plugin", "enabled", false);

// 导出/导入 JSON
std::string json = config.toJson();
config.fromJson(json);
```

### 版本约束 (VersionConstraint)

```cpp
// 解析版本
SemanticVersion v = SemanticVersion::parse("1.2.3");

// 版本约束
VersionConstraint c = VersionConstraint::parse("^1.2.3");
bool satisfied = c.isSatisfiedBy(SemanticVersion(1, 5, 0)); // true
```

## 运行测试

```bash
# 单元测试
./build/bin/plugin_tests

# 集成测试
./build/bin/test_plugin_host
```

## 注意事项

1. **链接选项**: 宿主程序需要 `-rdynamic` 选项以导出符号
2. **CMake 设置**:
   ```cmake
   set_target_properties(host_app PROPERTIES ENABLE_EXPORTS ON)
   target_link_options(host_app PRIVATE -rdynamic)
   ```
3. **插件生命周期**: 确保 `IPluginContext` 生命周期长于插件
4. **C++11 兼容**: 所有代码兼容 C++11 标准

## 许可证

MIT License
