/**
 * @file test_workflow_observer.cpp
 * @brief 工作流观察者模式测试
 */

#include "workflow_system/implementations/workflow_observer_impl.h"
#include "test_framework.h"
#include <algorithm>

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助类 ==========

class TestWorkflowObserver : public IWorkflowObserver {
private:
    int startedCount_ = 0;
    int finishedCount_ = 0;
    int interruptedCount_ = 0;
    int errorCount_ = 0;
    std::string lastWorkflowName_;
    std::string lastErrorMessage_;

public:
    void onWorkflowStarted(const std::string& workflowName) override {
        startedCount_++;
        lastWorkflowName_ = workflowName;
    }

    void onWorkflowFinished(const std::string& workflowName) override {
        finishedCount_++;
        lastWorkflowName_ = workflowName;
    }

    void onWorkflowInterrupted(const std::string& workflowName) override {
        interruptedCount_++;
        lastWorkflowName_ = workflowName;
    }

    void onWorkflowError(const std::string& workflowName, const std::string& error) override {
        errorCount_++;
        lastWorkflowName_ = workflowName;
        lastErrorMessage_ = error;
    }

    // Getter 方法
    int getStartedCount() const { return startedCount_; }
    int getFinishedCount() const { return finishedCount_; }
    int getInterruptedCount() const { return interruptedCount_; }
    int getErrorCount() const { return errorCount_; }
    std::string getLastWorkflowName() const { return lastWorkflowName_; }
    std::string getLastErrorMessage() const { return lastErrorMessage_; }

    // 重置计数器
    void reset() {
        startedCount_ = 0;
        finishedCount_ = 0;
        interruptedCount_ = 0;
        errorCount_ = 0;
        lastWorkflowName_ = "";
        lastErrorMessage_ = "";
    }
};

class TestSubject {
private:
    std::vector<std::shared_ptr<IWorkflowObserver>> observers_;

public:
    void addObserver(std::shared_ptr<IWorkflowObserver> observer) {
        observers_.push_back(observer);
    }

    void removeObserver(std::shared_ptr<IWorkflowObserver> observer) {
        auto it = std::find_if(observers_.begin(), observers_.end(),
            [&observer](const std::shared_ptr<IWorkflowObserver>& obs) {
                return obs.get() == observer.get();
            });
        if (it != observers_.end()) {
            observers_.erase(it);
        }
    }

    void notifyStarted(const std::string& workflowName) {
        for (auto& observer : observers_) {
            observer->onWorkflowStarted(workflowName);
        }
    }

    void notifyFinished(const std::string& workflowName) {
        for (auto& observer : observers_) {
            observer->onWorkflowFinished(workflowName);
        }
    }

    void notifyInterrupted(const std::string& workflowName) {
        for (auto& observer : observers_) {
            observer->onWorkflowInterrupted(workflowName);
        }
    }

    void notifyError(const std::string& workflowName, const std::string& error) {
        for (auto& observer : observers_) {
            observer->onWorkflowError(workflowName, error);
        }
    }
};

// ========== 测试用例 ==========

void testObserver_OnWorkflowStarted() {
    TestWorkflowObserver observer;

    observer.onWorkflowStarted("test_workflow");

    TEST_ASSERT_EQUAL(1, observer.getStartedCount());
    TEST_ASSERT_STRING_EQUAL("test_workflow", observer.getLastWorkflowName().c_str());
}

void testObserver_OnWorkflowFinished() {
    TestWorkflowObserver observer;

    observer.onWorkflowFinished("test_workflow");

    TEST_ASSERT_EQUAL(1, observer.getFinishedCount());
    TEST_ASSERT_STRING_EQUAL("test_workflow", observer.getLastWorkflowName().c_str());
}

void testObserver_OnWorkflowInterrupted() {
    TestWorkflowObserver observer;

    observer.onWorkflowInterrupted("test_workflow");

    TEST_ASSERT_EQUAL(1, observer.getInterruptedCount());
    TEST_ASSERT_STRING_EQUAL("test_workflow", observer.getLastWorkflowName().c_str());
}

void testObserver_OnWorkflowError() {
    TestWorkflowObserver observer;

    observer.onWorkflowError("test_workflow", "Something went wrong");

    TEST_ASSERT_EQUAL(1, observer.getErrorCount());
    TEST_ASSERT_STRING_EQUAL("test_workflow", observer.getLastWorkflowName().c_str());
    TEST_ASSERT_STRING_EQUAL("Something went wrong", observer.getLastErrorMessage().c_str());
}

void testObserver_MultipleNotifications() {
    TestWorkflowObserver observer;

    observer.onWorkflowStarted("workflow1");
    observer.onWorkflowFinished("workflow1");
    observer.onWorkflowStarted("workflow2");
    observer.onWorkflowFinished("workflow2");

    TEST_ASSERT_EQUAL(2, observer.getStartedCount());
    TEST_ASSERT_EQUAL(2, observer.getFinishedCount());
    TEST_ASSERT_STRING_EQUAL("workflow2", observer.getLastWorkflowName().c_str());
}

void testObserver_Subscription() {
    TestSubject subject;
    auto observer = std::make_shared<TestWorkflowObserver>();

    subject.addObserver(observer);
    subject.notifyStarted("test_workflow");

    TEST_ASSERT_EQUAL(1, observer->getStartedCount());
}

void testObserver_MultipleObservers() {
    TestSubject subject;
    auto observer1 = std::make_shared<TestWorkflowObserver>();
    auto observer2 = std::make_shared<TestWorkflowObserver>();
    auto observer3 = std::make_shared<TestWorkflowObserver>();

    subject.addObserver(observer1);
    subject.addObserver(observer2);
    subject.addObserver(observer3);

    subject.notifyStarted("test_workflow");

    TEST_ASSERT_EQUAL(1, observer1->getStartedCount());
    TEST_ASSERT_EQUAL(1, observer2->getStartedCount());
    TEST_ASSERT_EQUAL(1, observer3->getStartedCount());
}

void testObserver_RemoveObserver() {
    TestSubject subject;
    auto observer1 = std::make_shared<TestWorkflowObserver>();
    auto observer2 = std::make_shared<TestWorkflowObserver>();

    subject.addObserver(observer1);
    subject.addObserver(observer2);

    subject.notifyStarted("workflow1");
    TEST_ASSERT_EQUAL(1, observer1->getStartedCount());
    TEST_ASSERT_EQUAL(1, observer2->getStartedCount());

    subject.removeObserver(observer1);

    subject.notifyStarted("workflow2");
    TEST_ASSERT_EQUAL(1, observer1->getStartedCount());  // 没有增加
    TEST_ASSERT_EQUAL(2, observer2->getStartedCount());  // 增加了
}

void testObserver_WorkflowLifecycle() {
    TestSubject subject;
    auto observer = std::make_shared<TestWorkflowObserver>();

    subject.addObserver(observer);

    subject.notifyStarted("my_workflow");
    // ... 执行工作流 ...
    subject.notifyFinished("my_workflow");

    TEST_ASSERT_EQUAL(1, observer->getStartedCount());
    TEST_ASSERT_EQUAL(1, observer->getFinishedCount());
}

void testObserver_ErrorHandling() {
    TestSubject subject;
    auto observer = std::make_shared<TestWorkflowObserver>();

    subject.addObserver(observer);

    subject.notifyStarted("my_workflow");
    subject.notifyError("my_workflow", "Database connection failed");

    TEST_ASSERT_EQUAL(1, observer->getStartedCount());
    TEST_ASSERT_EQUAL(1, observer->getErrorCount());
    TEST_ASSERT_STRING_EQUAL("Database connection failed", observer->getLastErrorMessage().c_str());
}

void testObserver_Reset() {
    TestWorkflowObserver observer;

    observer.onWorkflowStarted("workflow1");
    observer.onWorkflowFinished("workflow1");
    observer.onWorkflowError("workflow1", "Error");

    TEST_ASSERT_EQUAL(1, observer.getStartedCount());
    TEST_ASSERT_EQUAL(1, observer.getFinishedCount());
    TEST_ASSERT_EQUAL(1, observer.getErrorCount());

    observer.reset();

    TEST_ASSERT_EQUAL(0, observer.getStartedCount());
    TEST_ASSERT_EQUAL(0, observer.getFinishedCount());
    TEST_ASSERT_EQUAL(0, observer.getErrorCount());
    TEST_ASSERT_TRUE(observer.getLastWorkflowName().empty());
    TEST_ASSERT_TRUE(observer.getLastErrorMessage().empty());
}

// ========== 测试套件创建 ==========

TestSuite createWorkflowObserverTestSuite() {
    TestSuite suite("工作流观察者模式测试");

    suite.addTest("Observer_OnWorkflowStarted", testObserver_OnWorkflowStarted);
    suite.addTest("Observer_OnWorkflowFinished", testObserver_OnWorkflowFinished);
    suite.addTest("Observer_OnWorkflowInterrupted", testObserver_OnWorkflowInterrupted);
    suite.addTest("Observer_OnWorkflowError", testObserver_OnWorkflowError);
    suite.addTest("Observer_MultipleNotifications", testObserver_MultipleNotifications);
    suite.addTest("Observer_Subscription", testObserver_Subscription);
    suite.addTest("Observer_MultipleObservers", testObserver_MultipleObservers);
    suite.addTest("Observer_RemoveObserver", testObserver_RemoveObserver);
    suite.addTest("Observer_WorkflowLifecycle", testObserver_WorkflowLifecycle);
    suite.addTest("Observer_ErrorHandling", testObserver_ErrorHandling);
    suite.addTest("Observer_Reset", testObserver_Reset);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowObserverTestSuite());

    return TestRunner::runAllSuites(suites);
}
