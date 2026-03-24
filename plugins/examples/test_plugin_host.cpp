/**
 * @file test_plugin_host.cpp
 * @brief 测试程序 - 宿主程序，动态加载插件并调用
 */

#include "plugin_interface.h"
#include "plugin_loader.h"
#include "service_registry.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>

using namespace WorkflowSystem::Plugin;

/**
 * @brief 示例服务接口 - 由宿主提供
 */
class ILoggerService {
public:
    virtual ~ILoggerService() = default;
    virtual void log(const std::string& level, const std::string& message) = 0;
    virtual std::string getName() const = 0;
};

/**
 * @brief 控制台日志服务实现
 */
class ConsoleLoggerService : public ILoggerService {
public:
    void log(const std::string& level, const std::string& message) override {
        std::cout << "[" << level << "] " << message << std::endl;
    }
    
    std::string getName() const override {
        return "ConsoleLogger";
    }
};

/**
 * @brief 数据库服务接口
 */
class IDatabaseService {
public:
    virtual ~IDatabaseService() = default;
    virtual bool connect(const std::string& connectionString) = 0;
    virtual std::string query(const std::string& sql) = 0;
    virtual std::string getName() const = 0;
};

/**
 * @brief 模拟数据库服务
 */
class MockDatabaseService : public IDatabaseService {
public:
    bool connect(const std::string& connectionString) override {
        connectionString_ = connectionString;
        connected_ = true;
        std::cout << "[MockDB] 连接到: " << connectionString << std::endl;
        return true;
    }
    
    std::string query(const std::string& sql) override {
        if (!connected_) {
            return "ERROR: 未连接";
        }
        return "RESULT: 执行查询 -> " + sql;
    }
    
    std::string getName() const override {
        return "MockDatabase";
    }
    
private:
    std::string connectionString_;
    bool connected_ = false;
};

void printSeparator() {
    std::cout << "\n========================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════╗\n"
              << "║     WorkflowSystem 插件系统测试         ║\n"
              << "╚════════════════════════════════════════╝" << std::endl;
    
    // 1. 注册宿主服务 - 这些服务可以被插件访问
    printSeparator();
    std::cout << "1. 注册宿主服务（依赖注入）" << std::endl;
    
    auto loggerService = std::make_shared<ConsoleLoggerService>();
    auto dbService = std::make_shared<MockDatabaseService>();
    
    // 注册到全局服务注册中心
    ServiceRegistry::getInstance().registerShared("logger", 
        std::static_pointer_cast<ILoggerService>(loggerService));
    ServiceRegistry::getInstance().registerShared("database", 
        std::static_pointer_cast<IDatabaseService>(dbService));
    
    // 初始化数据库
    dbService->connect("localhost:5432/workflow");
    
    std::cout << "已注册服务: ";
    auto services = ServiceRegistry::getInstance().listServices();
    for (const auto& name : services) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    // 2. 创建插件加载器
    printSeparator();
    std::cout << "2. 创建插件加载器" << std::endl;
    
    PluginLoader loader;
    
    // 设置插件搜索路径
    std::string pluginPath = "./plugins";
    if (argc > 1) {
        pluginPath = argv[1];
    }
    loader.addSearchPath(pluginPath);
    
    std::cout << "插件搜索路径: " << pluginPath << std::endl;
    
    // 设置事件回调
    loader.setEventCallback([](const std::string& pluginName, const std::string& event) {
        std::cout << "[EVENT] 插件 '" << pluginName << "' -> " << event << std::endl;
    });
    
    // 3. 扫描和加载插件
    printSeparator();
    std::cout << "3. 扫描和加载插件" << std::endl;
    
    auto discoveredPlugins = loader.scanPlugins();
    std::cout << "发现插件数量: " << discoveredPlugins.size() << std::endl;
    
    for (const auto& path : discoveredPlugins) {
        std::cout << "  - " << path << std::endl;
    }
    
    int loadedCount = loader.loadAll();
    std::cout << "成功加载插件数量: " << loadedCount << std::endl;
    
    // 4. 初始化插件（不传自定义 context，使用默认）
    printSeparator();
    std::cout << "4. 初始化插件" << std::endl;
    
    auto loadedPlugins = loader.getLoadedPlugins();
    for (const auto& name : loadedPlugins) {
        auto metadata = loader.getMetadata(name);
        std::cout << "初始化插件: " << name 
                  << " (v" << metadata.version.toString() << ")" << std::endl;
        
        if (loader.initialize(name, nullptr)) {
            std::cout << "  ✓ 初始化成功" << std::endl;
        } else {
            std::cout << "  ✗ 初始化失败" << std::endl;
        }
    }
    
    // 5. 启动插件
    printSeparator();
    std::cout << "5. 启动插件" << std::endl;
    
    int startedCount = loader.startAll();
    std::cout << "成功启动插件数量: " << startedCount << std::endl;
    
    // 6. 执行插件功能
    printSeparator();
    std::cout << "6. 执行插件功能" << std::endl;
    
    for (const auto& name : loadedPlugins) {
        std::cout << "\n--- 插件: " << name << " ---" << std::endl;
        
        IPlugin* plugin = loader.getPlugin(name);
        if (!plugin) continue;
        
        // 获取支持的动作
        auto actions = plugin->getSupportedActions();
        std::cout << "支持的动作: ";
        for (const auto& action : actions) {
            std::cout << action << " ";
        }
        std::cout << std::endl;
        
        // 执行 greet 动作
        PluginParams greetParams;
        greetParams.action = "greet";
        greetParams.args["name"] = "WorkflowSystem";
        
        PluginResult result = loader.execute(name, greetParams);
        std::cout << "greet 结果: " << result.message;
        if (!result.data.empty()) {
            std::cout << " -> " << result.data;
        }
        std::cout << std::endl;
        
        // 执行 echo 动作
        PluginParams echoParams;
        echoParams.action = "echo";
        echoParams.args["message"] = "测试消息";
        echoParams.intArgs["count"] = 42;
        echoParams.boolArgs["flag"] = true;
        
        result = loader.execute(name, echoParams);
        std::cout << "echo 结果: " << result.message;
        if (!result.data.empty()) {
            std::cout << " -> " << result.data;
        }
        std::cout << std::endl;
        
        // 执行 use_service 动作 - 让插件使用宿主服务
        PluginParams serviceParams;
        serviceParams.action = "use_service";
        serviceParams.args["service"] = "logger";
        
        result = loader.execute(name, serviceParams);
        std::cout << "use_service(logger) 结果: " << result.message;
        if (!result.data.empty()) {
            std::cout << " -> " << result.data;
        }
        std::cout << std::endl;
        
        serviceParams.args["service"] = "database";
        result = loader.execute(name, serviceParams);
        std::cout << "use_service(database) 结果: " << result.message;
        if (!result.data.empty()) {
            std::cout << " -> " << result.data;
        }
        std::cout << std::endl;
        
        // 健康检查
        result = plugin->healthCheck();
        std::cout << "健康检查: " << (result.success ? "通过" : "失败") << std::endl;
    }
    
    // 7. 停止和卸载插件
    printSeparator();
    std::cout << "7. 停止和卸载插件" << std::endl;
    
    int stoppedCount = loader.stopAll();
    std::cout << "停止插件数量: " << stoppedCount << std::endl;
    
    loader.unloadAll();
    std::cout << "已卸载所有插件" << std::endl;
    
    // 8. 清理服务
    printSeparator();
    std::cout << "8. 清理" << std::endl;
    
    ServiceRegistry::getInstance().clear();
    std::cout << "已清理服务注册中心" << std::endl;
    
    printSeparator();
    std::cout << "测试完成！" << std::endl;
    
    return 0;
}
