/**
 * @file workflow.h
 * @brief 业务流程管理系统 - 工作流接口
 *
 * 设计模式：
 * - 策略模式：不同工作流有不同的行为策略
 * - 模板方法模式：在基类中定义算法骨架
 *
 * 面向对象：
 * - 封装：每个工作流管理自己的状态和行为
 * - 多态：不同工作流实现接口
 */

#ifndef WORKFLOW_SYSTEM_WORKFLOW_H
#define WORKFLOW_SYSTEM_WORKFLOW_H

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>
#include <future>
#include "workflow_system/core/types.h"
#include "workflow_system/core/any.h"
#include "workflow_observer.h"

namespace WorkflowSystem {

// 前向声明
class IWorkflowContext;

/**
 * @brief 通用消息接口
 *
 * 提供通用消息处理接口
 * 支持不同类型的消息协议
 */
class IMessage {
public:
    virtual ~IMessage() = default;

    virtual std::string getType() const = 0;
    virtual std::string getTopic() const = 0;
    virtual std::string getPayload() const = 0;
};

/**
 * @brief 工作流消息处理器类型
 *
 * 定义工作流可以接收的回调类型
 */
using MessageHandler = std::function<void(const IMessage& message)>;

/**
 * @brief 工作流接口
 *
 * 设计模式：策略模式
 * 定义基本工作流行为
 *
 * 注意：工作流不再继承观察者接口，而是通过组合方式管理观察者列表
 * 工作流和观察者是不同的概念，不应该混在一起
 */
class IWorkflow {
public:
    virtual ~IWorkflow() = default;

    virtual std::string getName() const = 0;
    virtual WorkflowState getState() const = 0;

    // 工作流生命周期
    virtual void start(std::shared_ptr<IWorkflowContext> context) = 0;
    virtual void handleMessage(const IMessage& message) = 0;
    virtual void interrupt() = 0;
    virtual void cancel() = 0;
    virtual void finish() = 0;
    virtual void error(const std::string& errorMsg) = 0;

    // 执行工作流主体（在新线程中）
    virtual std::future<Any> execute() = 0;

    // 上下文管理
    virtual void setContext(std::shared_ptr<IWorkflowContext> context) = 0;
    virtual std::shared_ptr<IWorkflowContext> getContext() const = 0;

    // 观察者管理
    virtual void addObserver(std::shared_ptr<IWorkflowObserver> observer) = 0;
    virtual void removeObserver(std::shared_ptr<IWorkflowObserver> observer) = 0;

    // 消息处理设置
    virtual void setMessageHandler(MessageHandler handler) = 0;
};

/**
 * @brief 工作流上下文接口
 *
 * 职责：在工作流之间传递数据和资源
 */
class IWorkflowContext {
public:
    virtual ~IWorkflowContext() = default;

    // 字符串数据存储
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key, const std::string& defaultValue = "") const = 0;

    // 对象存储（支持任意类型）
    template<typename T>
    void setObject(const std::string& key, const T& value) {
        setObjectImpl(key, Any(value));
    }

    template<typename T>
    T getObject(const std::string& key) const {
        return getObjectImpl(key).cast<T>();
    }

    template<typename T>
    T getObject(const std::string& key, const T& defaultValue) const {
        auto any = getObjectImpl(key);
        if (!any.has_value()) {
            return defaultValue;
        }
        try {
            return any.cast<T>();
        } catch (...) {
            return defaultValue;
        }
    }

    // 检查键是否存在
    virtual bool has(const std::string& key) const = 0;

    // 工作目录管理
    virtual std::string getWorkingDir() const = 0;
    virtual void setWorkingDir(const std::string& path) = 0;

    // 资源清理
    virtual void cleanup() = 0;

    // 数据导出
    virtual std::unordered_map<std::string, std::string> getAllData() const = 0;

    // 获取所有对象（类型擦除）
    virtual std::unordered_map<std::string, Any> getAllObjects() const = 0;

    // 对象存储虚函数（模板方法需要在基类可见）
    virtual void setObjectImpl(const std::string& key, const Any& value) = 0;
    virtual Any getObjectImpl(const std::string& key) const = 0;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_WORKFLOW_H
