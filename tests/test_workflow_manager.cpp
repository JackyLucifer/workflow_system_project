/**
 * @file test_workflow_manager.cpp
 * @brief 工作流管理器测试
 */

#include "workflow_system/implementations/workflow_manager.h"
#include "workflow_system/implementations/workflow_context.h"
#include "workflow_system/interfaces/workflow.h"
#include "workflow_system/implementations/filesystem_resource_manager.h"
#include "test_framework.h"

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助类 ==========

class TestWorkflow : public IWorkflow {
private:
    std::string name_;
    WorkflowState state_ = WorkflowState::IDLE;
    bool executed_ = false;
    std::shared_ptr<IWorkflowContext> context_;

public:
    TestWorkflow(const std::string& name) : name_(name) {}

    std::string getName() const override { return name_; }
    WorkflowState getState() const override { return state_; }

    void start(std::shared_ptr<IWorkflowContext> context) override {
        context_ = context;
        state_ = WorkflowState::RUNNING;
    }

    std::future<Any> execute() override {
        executed_ = true;
        state_ = WorkflowState::COMPLETED;

        std::promise<Any> p;
        p.set_value(Any(std::string("success")));
        return p.get_future();
    }

    void finish() override { state_ = WorkflowState::COMPLETED; }
    void interrupt() override { state_ = WorkflowState::INTERRUPTED; }
    void cancel() override { state_ = WorkflowState::CANCELED; }

    void handleMessage(const IMessage& message) override {}
    void error(const std::string& errorMsg) override {}

    std::shared_ptr<IWorkflowContext> getContext() const override { return context_; }
    void setContext(std::shared_ptr<IWorkflowContext> context) override { context_ = context; }

    void addObserver(std::shared_ptr<IWorkflowObserver> observer) override {}
    void removeObserver(std::shared_ptr<IWorkflowObserver> observer) override {}
    void setMessageHandler(MessageHandler handler) override {}

    bool isExecuted() const { return executed_; }
};

// ========== 测试用例 ==========

void testWorkflowManager_RegisterWorkflow() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    bool result = manager.registerWorkflow("test_workflow", workflow);

    TEST_ASSERT_TRUE(result);

    auto retrieved = manager.getWorkflow("test_workflow");
    TEST_ASSERT_TRUE(retrieved != nullptr);
    TEST_ASSERT_STRING_EQUAL("test_workflow", retrieved->getName().c_str());
}

void testWorkflowManager_UnregisterWorkflow() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);
    TEST_ASSERT_TRUE(manager.getWorkflow("test_workflow") != nullptr);

    bool result = manager.unregisterWorkflow("test_workflow");

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(manager.getWorkflow("test_workflow") == nullptr);
}

void testWorkflowManager_StartWorkflow() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);

    bool result = manager.startWorkflow("test_workflow");

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(workflow->getState() == WorkflowState::RUNNING);
}

void testWorkflowManager_SwitchWorkflow() {
    WorkflowManager manager;
    auto workflow1 = std::make_shared<TestWorkflow>("workflow1");
    auto workflow2 = std::make_shared<TestWorkflow>("workflow2");

    manager.registerWorkflow("workflow1", workflow1);
    manager.registerWorkflow("workflow2", workflow2);

    manager.startWorkflow("workflow1");

    bool result = manager.switchWorkflow("workflow2", false);

    TEST_ASSERT_TRUE(result);

    auto current = manager.getCurrentWorkflow();
    TEST_ASSERT_STRING_EQUAL("workflow2", current->getName().c_str());
}

void testWorkflowManager_SwitchWorkflowPreserveContext() {
    WorkflowManager manager;
    auto workflow1 = std::make_shared<TestWorkflow>("workflow1");
    auto workflow2 = std::make_shared<TestWorkflow>("workflow2");

    manager.registerWorkflow("workflow1", workflow1);
    manager.registerWorkflow("workflow2", workflow2);

    manager.startWorkflow("workflow1");

    bool result = manager.switchWorkflow("workflow2", true);

    TEST_ASSERT_TRUE(result);
}

void testWorkflowManager_Interrupt() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);
    manager.startWorkflow("test_workflow");

    manager.interrupt();

    auto current = manager.getCurrentWorkflow();
    TEST_ASSERT_TRUE(current == nullptr || current->getState() == WorkflowState::INTERRUPTED);
}

void testWorkflowManager_Cancel() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);
    manager.startWorkflow("test_workflow");

    manager.cancel();

    auto current = manager.getCurrentWorkflow();
    TEST_ASSERT_TRUE(current == nullptr || current->getState() == WorkflowState::CANCELED);
}

void testWorkflowManager_GetCurrentWorkflow() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    TEST_ASSERT_TRUE(manager.getCurrentWorkflow() == nullptr);

    manager.registerWorkflow("test_workflow", workflow);
    manager.startWorkflow("test_workflow");

    auto current = manager.getCurrentWorkflow();
    TEST_ASSERT_TRUE(current != nullptr);
    TEST_ASSERT_STRING_EQUAL("test_workflow", current->getName().c_str());
}

void testWorkflowManager_GetStatus() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);

    std::string status = manager.getStatus();

    TEST_ASSERT_FALSE(status.empty());
    TEST_ASSERT_TRUE(status.find("test_workflow") != std::string::npos);
}

void testWorkflowManager_GetResourceManager() {
    WorkflowManager manager;

    auto rm = manager.getResourceManager();

    TEST_ASSERT_TRUE(rm != nullptr);
}

void testWorkflowManager_CleanupAllResources() {
    WorkflowManager manager;

    // 这个测试主要验证不会崩溃
    manager.cleanupAllResources();

    TEST_ASSERT_TRUE(true);
}

void testWorkflowManager_SetDefaultTimeout() {
    WorkflowManager manager;

    manager.setDefaultTimeout(5000);

    TEST_ASSERT_EQUAL(5000, manager.getDefaultTimeout());
}

void testWorkflowManager_EnqueueWorkflow() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);

    WorkflowTask task;
    task.taskId = "task_001";
    task.workflowName = "test_workflow";

    manager.enqueueWorkflow(task);

    TEST_ASSERT_EQUAL(1, static_cast<int>(manager.getQueueSize()));
}

void testWorkflowManager_DequeueWorkflow() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);

    WorkflowTask task;
    task.taskId = "task_001";
    task.workflowName = "test_workflow";

    manager.enqueueWorkflow(task);
    TEST_ASSERT_EQUAL(1, static_cast<int>(manager.getQueueSize()));

    manager.dequeueWorkflow("task_001");
    TEST_ASSERT_EQUAL(0, static_cast<int>(manager.getQueueSize()));
}

void testWorkflowManager_GetQueue() {
    WorkflowManager manager;
    auto workflow = std::make_shared<TestWorkflow>("test_workflow");

    manager.registerWorkflow("test_workflow", workflow);

    WorkflowTask task1;
    task1.taskId = "task_001";
    task1.workflowName = "test_workflow";

    WorkflowTask task2;
    task2.taskId = "task_002";
    task2.workflowName = "test_workflow";

    manager.enqueueWorkflow(task1);
    manager.enqueueWorkflow(task2);

    auto queue = manager.getQueue();

    TEST_ASSERT_EQUAL(2, static_cast<int>(queue.size()));
}

void testWorkflowManager_EnablePersistence() {
    WorkflowManager manager;

    std::string dbPath = "/tmp/test_workflow_manager_persistence.db";
    bool result = manager.enablePersistence(dbPath);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(manager.isPersistenceEnabled());

    manager.disablePersistence();

    unlink(dbPath.c_str());
}

void testWorkflowManager_DisablePersistence() {
    WorkflowManager manager;

    std::string dbPath = "/tmp/test_workflow_manager_persistence.db";
    manager.enablePersistence(dbPath);
    TEST_ASSERT_TRUE(manager.isPersistenceEnabled());

    manager.disablePersistence();

    TEST_ASSERT_FALSE(manager.isPersistenceEnabled());

    unlink(dbPath.c_str());
}

// ========== 测试套件创建 ==========

TestSuite createWorkflowManagerTestSuite() {
    TestSuite suite("工作流管理器测试");

    suite.addTest("WorkflowManager_RegisterWorkflow", testWorkflowManager_RegisterWorkflow);
    suite.addTest("WorkflowManager_UnregisterWorkflow", testWorkflowManager_UnregisterWorkflow);
    suite.addTest("WorkflowManager_StartWorkflow", testWorkflowManager_StartWorkflow);
    suite.addTest("WorkflowManager_SwitchWorkflow", testWorkflowManager_SwitchWorkflow);
    suite.addTest("WorkflowManager_SwitchWorkflowPreserveContext", testWorkflowManager_SwitchWorkflowPreserveContext);
    suite.addTest("WorkflowManager_Interrupt", testWorkflowManager_Interrupt);
    suite.addTest("WorkflowManager_Cancel", testWorkflowManager_Cancel);
    suite.addTest("WorkflowManager_GetCurrentWorkflow", testWorkflowManager_GetCurrentWorkflow);
    suite.addTest("WorkflowManager_GetStatus", testWorkflowManager_GetStatus);
    suite.addTest("WorkflowManager_GetResourceManager", testWorkflowManager_GetResourceManager);
    suite.addTest("WorkflowManager_CleanupAllResources", testWorkflowManager_CleanupAllResources);
    suite.addTest("WorkflowManager_SetDefaultTimeout", testWorkflowManager_SetDefaultTimeout);
    suite.addTest("WorkflowManager_EnqueueWorkflow", testWorkflowManager_EnqueueWorkflow);
    suite.addTest("WorkflowManager_DequeueWorkflow", testWorkflowManager_DequeueWorkflow);
    suite.addTest("WorkflowManager_GetQueue", testWorkflowManager_GetQueue);
    suite.addTest("WorkflowManager_EnablePersistence", testWorkflowManager_EnablePersistence);
    suite.addTest("WorkflowManager_DisablePersistence", testWorkflowManager_DisablePersistence);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowManagerTestSuite());

    return TestRunner::runAllSuites(suites);
}
