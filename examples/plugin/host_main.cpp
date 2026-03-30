/**
 * @file host_main.cpp
 * @brief 完整的宿主程序示例
 * 
 * 演示：
 * 1. 框架初始化
 * 2. 服务注册（注入）
 * 3. 插件发现和加载
 * 4. 异步通讯
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <signal.h>

#include "workflow_system/plugin/core/IPluginManager.hpp"
#include "workflow_system/plugin/core/IPluginContext.hpp"
#include "workflow_system/plugin/core/ServiceLocator.hpp"
#include "workflow_system/plugin/communication/Channel.hpp"
#include "workflow_system/core/logger.h"

using namespace WorkflowSystem::Plugin;

// ==================== 服务实现 ====================

/**
 * @brief 日志服务实现
 */
class ConsoleLogService {
public:
    void info(const std::string& msg) {
        std::cout << "\033[32m[INFO] " << msg << "\033[0m" << std::endl;
    }
    
    void error(const std::string& msg) {
        std::cout << "\033[31m[ERROR] " << msg << "\033[0m" << std::endl;
    }
    
    void warning(const std::string& msg) {
        std::cout << "\033[33m[WARN] " << msg << "\033[0m" << std::endl;
    }
    
    void debug(const std::string& msg) {
        std::cout << "\033[36m[DEBUG] " << msg << "\033[0m" << std::endl;
    }
};

/**
 * @brief 配置服务实现
 */
class ConfigService {
public:
    ConfigService() {
        // 默认配置
        config_["mode"] = "production";
        config_["log_level"] = "info";
        config_["max_connections"] = "100";
    }
    
    std::string get(const std::string& key, const std::string& def = "") {
        auto it = config_.find(key);
        return (it != config_.end()) ? it->second : def;
    }
    
    void set(const std::string& key, const std::string& value) {
        config_[key] = value;
        std::cout << "[ConfigService] 配置更新: " << key << " = " << value << std::endl;
    }
    
    void reload() {
        std::cout << "[ConfigService] 配置已重新加载" << std::endl;
    }

private:
    std::map<std::string, std::string> config_;
};

/**
 * @brief 数据库服务实现
 */
class DatabaseService {
public:
    DatabaseService() : connected_(false) {}
    
    bool connect(const std::string& connectionString) {
        connectionString_ = connectionString;
        connected_ = true;
        std::cout << "[DatabaseService] 已连接: " << connectionString << std::endl;
        return true;
    }
    
    void disconnect() {
        connected_ = false;
        std::cout << "[DatabaseService] 已断开连接" << std::endl;
    }
    
    bool isConnected() const { return connected_; }
    
    std::string query(const std::string& sql) {
        if (!connected_) return "error: not connected";
        return "result for: " + sql;
    }

private:
    std::string connectionString_;
    bool connected_;
};

// ==================== 宿主应用 ====================

class HostApplication {
public:
    HostApplication() : running_(false) {}
    
    bool initialize() {
        std::cout << "========================================" << std::endl;
        std::cout << "  插件框架宿主程序 v1.0" << std::endl;
        std::cout << "========================================" << std::endl << std::endl;
        
        // 1. 初始化日志
        WorkflowSystem::Logger::getInstance().setLevel(WorkflowSystem::LogLevel::INFO);
        
        // 2. 注册服务（在插件加载前）
        registerServices();
        
        // 3. 初始化插件管理器
        if (!initializePluginManager()) {
            std::cerr << "初始化插件管理器失败" << std::endl;
            return false;
        }
        
        // 4. 设置全局通讯通道
        setupChannels();
        
        // 5. 发现和加载插件
        discoverAndLoadPlugins();
        
        running_ = true;
        return true;
    }
    
    void run() {
        std::cout << std::endl;
        std::cout << "宿主程序已启动，输入命令 (h=帮助, q=退出)" << std::endl;
        std::cout << std::endl;
        
        std::string input;
        while (running_) {
            std::cout << "> ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            handleCommand(input);
        }
    }
    
    void shutdown() {
        std::cout << std::endl;
        std::cout << "正在关闭宿主程序..." << std::endl;
        
        // 停止所有插件
        if (pluginManager_) {
            pluginManager_->stopAllPlugins();
        }
        
        // 关闭通道
        ChannelManager::instance().closeAll();
        
        // 清理服务
        ServiceLocator::instance().clear();
        
        std::cout << "宿主程序已关闭" << std::endl;
    }

private:
    std::shared_ptr<ConsoleLogService> logService_;
    std::shared_ptr<ConfigService> configService_;
    std::shared_ptr<DatabaseService> dbService_;
    IPluginManager* pluginManager_;
    std::shared_ptr<Channel> mainChannel_;
    std::atomic<bool> running_;
    
    void registerServices() {
        std::cout << "--- 注册服务 ---" << std::endl;
        
        // 创建服务实例
        logService_ = std::shared_ptr<ConsoleLogService>(new ConsoleLogService());
        configService_ = std::shared_ptr<ConfigService>(new ConfigService());
        dbService_ = std::shared_ptr<DatabaseService>(new DatabaseService());
        
        // 连接数据库
        dbService_->connect("localhost:5432/mydb");
        
        // 注册到服务定位器
        ServiceLocator::instance().registerTyped<ConsoleLogService>("log_service", logService_.get());
        ServiceLocator::instance().registerTyped<ConfigService>("config_service", configService_.get());
        ServiceLocator::instance().registerTyped<DatabaseService>("database", dbService_.get());
        
        std::cout << "  - log_service (ConsoleLogService)" << std::endl;
        std::cout << "  - config_service (ConfigService)" << std::endl;
        std::cout << "  - database (DatabaseService)" << std::endl;
        std::cout << std::endl;
    }
    
    bool initializePluginManager() {
        // 直接创建插件管理器
        pluginManager_ = nullptr; // 暂不使用，演示代码
        return true;
    }
    
    void setupChannels() {
        std::cout << "--- 设置通讯通道 ---" << std::endl;
        
        // 获取默认通道
        mainChannel_ = ChannelManager::instance().getDefault();
        
        // 订阅所有插件事件
        mainChannel_->subscribe("plugin.*", [this](const DataPacket& packet) {
            handlePluginEvent(packet);
        });
        
        // 订阅系统事件
        mainChannel_->subscribe("system.*", [this](const DataPacket& packet) {
            handleSystemEvent(packet);
        });
        
        std::cout << "  - 默认通道已创建" << std::endl;
        std::cout << "  - 已订阅 plugin.* 和 system.* 主题" << std::endl;
        std::cout << std::endl;
    }
    
    void discoverAndLoadPlugins() {
        std::cout << "--- 发现和加载插件 ---" << std::endl;
        
        // 扫描插件目录
        std::vector<std::string> pluginDirs = {
            "./plugins",
            "./build/lib"
        };
        
        int loaded = 0;
        for (const auto& dir : pluginDirs) {
            // 尝试加载目录中的动态库
            // 这里简化处理，实际应该扫描plugin.json
            
            // 尝试加载演示插件
            std::string demoPluginPath = dir + "/libdemo_plugin.so";
            
            // 检查文件是否存在
            std::ifstream f(demoPluginPath);
            if (f.good()) {
                f.close();
                
                if (pluginManager_->loadPlugin(demoPluginPath).success) {
                    std::cout << "  已加载: " << demoPluginPath << std::endl;
                    loaded++;
                }
            }
        }
        
        if (loaded == 0) {
            std::cout << "  未找到插件" << std::endl;
        }
        
        std::cout << std::endl;
        
        // 启动所有插件
        if (pluginManager_) {
            pluginManager_->startAllPlugins();
        }
    }
    
    void handlePluginEvent(const DataPacket& packet) {
        logService_->info("收到插件事件: " + packet.topic + " 来自 " + packet.source);
        
        // 打印事件数据
        for (const auto& pair : packet.data) {
            logService_->debug("  " + pair.first + " = " + pair.second);
        }
    }
    
    void handleSystemEvent(const DataPacket& packet) {
        logService_->info("收到系统事件: " + packet.topic);
    }
    
    void handleCommand(const std::string& input) {
        char cmd = input[0];
        std::string args = input.size() > 1 ? input.substr(2) : "";
        
        switch (cmd) {
            case 'h':
            case 'H':
                printHelp();
                break;
                
            case 'q':
            case 'Q':
                running_ = false;
                break;
                
            case 'l':
            case 'L':
                listPlugins();
                break;
                
            case 's':
            case 'S':
                sendTestMessage(args);
                break;
                
            case 'p':
            case 'P':
                publishTestEvent(args);
                break;
                
            case 'c':
            case 'C':
                showConfig();
                break;
                
            case 'd':
            case 'D':
                testDatabase();
                break;
                
            default:
                std::cout << "未知命令: " << cmd << " (输入 h 查看帮助)" << std::endl;
        }
    }
    
    void printHelp() {
        std::cout << std::endl;
        std::cout << "可用命令:" << std::endl;
        std::cout << "  h        - 显示帮助" << std::endl;
        std::cout << "  l        - 列出已加载插件" << std::endl;
        std::cout << "  s <msg>  - 发送测试消息" << std::endl;
        std::cout << "  p <topic>- 发布测试事件" << std::endl;
        std::cout << "  c        - 显示配置" << std::endl;
        std::cout << "  d        - 测试数据库" << std::endl;
        std::cout << "  q        - 退出" << std::endl;
        std::cout << std::endl;
    }
    
    void listPlugins() {
        std::cout << std::endl;
        std::cout << "已加载插件:" << std::endl;
        
        auto plugins = pluginManager_->getAllPlugins();
        if (plugins.empty()) {
            std::cout << "  (无)" << std::endl;
        } else {
            for (auto* plugin : plugins) {
                auto spec = plugin->getSpec();
                std::cout << "  - " << spec.name << " v" << spec.version.toString() << std::endl;
                std::cout << "    " << spec.description << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    void sendTestMessage(const std::string& msg) {
        if (!mainChannel_) return;
        
        DataPacket packet;
        packet.topic = "test.message";
        packet.source = "host";
        packet.set("content", msg.empty() ? "Hello from host!" : msg);
        
        mainChannel_->publish(packet);
        std::cout << "已发送消息: " << packet.get("content") << std::endl;
    }
    
    void publishTestEvent(const std::string& topic) {
        if (!mainChannel_) return;
        
        std::string actualTopic = topic.empty() ? "user.created" : topic;
        
        DataPacket packet;
        packet.topic = actualTopic;
        packet.source = "host";
        packet.set("action", "test");
        packet.set("user_id", "12345");
        packet.set("timestamp", std::to_string(std::time(nullptr)));
        
        mainChannel_->publish(packet);
        std::cout << "已发布事件: " << actualTopic << std::endl;
    }
    
    void showConfig() {
        std::cout << std::endl;
        std::cout << "当前配置:" << std::endl;
        std::cout << "  mode = " << configService_->get("mode") << std::endl;
        std::cout << "  log_level = " << configService_->get("log_level") << std::endl;
        std::cout << "  max_connections = " << configService_->get("max_connections") << std::endl;
        std::cout << std::endl;
    }
    
    void testDatabase() {
        std::cout << std::endl;
        if (dbService_->isConnected()) {
            std::string result = dbService_->query("SELECT * FROM users LIMIT 10");
            std::cout << "数据库查询结果: " << result << std::endl;
        } else {
            std::cout << "数据库未连接" << std::endl;
        }
        std::cout << std::endl;
    }
};

// 全局指针用于信号处理
HostApplication* g_app = nullptr;

void signalHandler(int signum) {
    std::cout << std::endl << "收到信号 " << signum << std::endl;
    if (g_app) {
        g_app->shutdown();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    HostApplication app;
    g_app = &app;
    
    if (!app.initialize()) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }
    
    app.run();
    app.shutdown();
    
    return 0;
}
