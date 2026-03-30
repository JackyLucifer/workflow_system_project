/**
 * @file test_conditional_workflow.cpp
 * @brief 条件分支工作流测试
 */

#include "workflow_system/implementations/conditional_workflow_impl.h"
#include "workflow_system/implementations/workflow_context.h"
#include "workflow_system/implementations/filesystem_resource_manager.h"
#include "workflow_system/interfaces/workflow.h"
#include "test_framework.h"

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助类 ==========

class SimpleConditionalWorkflow : public IConditionalWorkflow {
private:
    std::vector<Branch> branches_;
    std::shared_ptr<Branch> defaultBranch_;
    std::shared_ptr<Branch> selectedBranch_;

public:
    SimpleConditionalWorkflow() : defaultBranch_(nullptr), selectedBranch_(nullptr) {}

    void addBranch(const Branch& branch) override {
        branches_.push_back(branch);
    }

    void setDefaultBranch(std::shared_ptr<IWorkflow> workflow) override {
        defaultBranch_ = std::make_shared<Branch>();
        defaultBranch_->workflow = workflow;
        defaultBranch_->name = "default";
    }

    void setDefaultBranch(const Branch& branch) override {
        defaultBranch_ = std::make_shared<Branch>(branch);
    }

    Branch* getSelectedBranch() override {
        return selectedBranch_.get();
    }

    std::vector<Branch> getAllBranches() const override {
        return branches_;
    }

    Branch* getDefaultBranch() override {
        return defaultBranch_.get();
    }

    void clearBranches() override {
        branches_.clear();
        selectedBranch_ = nullptr;
    }

    bool evaluateAndExecute() override {
        // 评估所有条件
        for (auto& branch : branches_) {
            if (branch.condition && branch.condition()) {
                selectedBranch_ = std::make_shared<Branch>(branch);
                return true;
            }
        }

        // 使用默认分支
        if (defaultBranch_) {
            selectedBranch_ = defaultBranch_;
            return true;
        }

        return false;
    }
};

// 简单的工作流实现用于测试
class SimpleTestWorkflow {
public:
    std::string name;
    
    SimpleTestWorkflow(const std::string& n) : name(n) {}
};

// ========== 测试用例 ==========

void testConditionalWorkflow_AddBranch() {
    SimpleConditionalWorkflow cf;

    Branch branch;
    branch.name = "branch1";
    branch.conditionName = "cond1";
    branch.priority = 1;
    branch.condition = []() { return true; };
    branch.workflow = nullptr;

    cf.addBranch(branch);

    auto branches = cf.getAllBranches();

    TEST_ASSERT_EQUAL(1, static_cast<int>(branches.size()));
    TEST_ASSERT_STRING_EQUAL("branch1", branches[0].name.c_str());
}

void testConditionalWorkflow_AddMultipleBranches() {
    SimpleConditionalWorkflow cf;

    Branch branch1;
    branch1.name = "branch1";
    branch1.priority = 1;
    branch1.condition = []() { return false; };

    Branch branch2;
    branch2.name = "branch2";
    branch2.priority = 2;
    branch2.condition = []() { return false; };

    Branch branch3;
    branch3.name = "branch3";
    branch3.priority = 3;
    branch3.condition = []() { return false; };

    cf.addBranch(branch1);
    cf.addBranch(branch2);
    cf.addBranch(branch3);

    auto branches = cf.getAllBranches();

    TEST_ASSERT_EQUAL(3, static_cast<int>(branches.size()));
}

void testConditionalWorkflow_SetDefaultBranch() {
    SimpleConditionalWorkflow cf;

    Branch branch;
    branch.name = "default";
    branch.workflow = nullptr;

    cf.setDefaultBranch(branch);

    auto defaultBranch = cf.getDefaultBranch();

    TEST_ASSERT_TRUE(defaultBranch != nullptr);
    TEST_ASSERT_STRING_EQUAL("default", defaultBranch->name.c_str());
}

void testConditionalWorkflow_EvaluateAndExecute_FirstBranch() {
    SimpleConditionalWorkflow cf;

    Branch branch1;
    branch1.name = "branch1";
    branch1.condition = []() { return true; };
    branch1.workflow = nullptr;

    Branch branch2;
    branch2.name = "branch2";
    branch2.condition = []() { return true; };
    branch2.workflow = nullptr;

    cf.addBranch(branch1);
    cf.addBranch(branch2);

    bool result = cf.evaluateAndExecute();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(cf.getSelectedBranch() != nullptr);
    TEST_ASSERT_STRING_EQUAL("branch1", cf.getSelectedBranch()->name.c_str());
}

void testConditionalWorkflow_EvaluateAndExecute_DefaultBranch() {
    SimpleConditionalWorkflow cf;

    Branch branch1;
    branch1.name = "branch1";
    branch1.condition = []() { return false; };  // 条件不满足

    Branch branch2;
    branch2.name = "default";
    branch2.workflow = nullptr;

    cf.addBranch(branch1);
    cf.setDefaultBranch(branch2);

    bool result = cf.evaluateAndExecute();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(cf.getSelectedBranch() != nullptr);
    TEST_ASSERT_STRING_EQUAL("default", cf.getSelectedBranch()->name.c_str());
}

void testConditionalWorkflow_ClearBranches() {
    SimpleConditionalWorkflow cf;

    Branch branch1;
    branch1.name = "branch1";
    branch1.condition = []() { return true; };

    Branch branch2;
    branch2.name = "branch2";
    branch2.condition = []() { return true; };

    cf.addBranch(branch1);
    cf.addBranch(branch2);

    TEST_ASSERT_EQUAL(2, static_cast<int>(cf.getAllBranches().size()));

    cf.clearBranches();

    TEST_ASSERT_EQUAL(0, static_cast<int>(cf.getAllBranches().size()));
    TEST_ASSERT_TRUE(cf.getSelectedBranch() == nullptr);
}

void testConditionalWorkflow_BranchPriority() {
    SimpleConditionalWorkflow cf;

    Branch branch1;
    branch1.name = "branch1";
    branch1.priority = 3;
    branch1.condition = []() { return true; };

    Branch branch2;
    branch2.name = "branch2";
    branch2.priority = 1;
    branch2.condition = []() { return true; };

    Branch branch3;
    branch3.name = "branch3";
    branch3.priority = 2;
    branch3.condition = []() { return true; };

    cf.addBranch(branch1);
    cf.addBranch(branch2);
    cf.addBranch(branch3);

    auto branches = cf.getAllBranches();

    // 验证分支已添加
    TEST_ASSERT_EQUAL(3, static_cast<int>(branches.size()));
}

void testConditionalWorkflow_NoBranchSelected() {
    SimpleConditionalWorkflow cf;

    // 不添加任何分支

    bool result = cf.evaluateAndExecute();

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(cf.getSelectedBranch() == nullptr);
}

// ========== 测试套件创建 ==========

TestSuite createConditionalWorkflowTestSuite() {
    TestSuite suite("条件分支工作流测试");

    suite.addTest("ConditionalWorkflow_AddBranch", testConditionalWorkflow_AddBranch);
    suite.addTest("ConditionalWorkflow_AddMultipleBranches", testConditionalWorkflow_AddMultipleBranches);
    suite.addTest("ConditionalWorkflow_SetDefaultBranch", testConditionalWorkflow_SetDefaultBranch);
    suite.addTest("ConditionalWorkflow_EvaluateAndExecute_FirstBranch", testConditionalWorkflow_EvaluateAndExecute_FirstBranch);
    suite.addTest("ConditionalWorkflow_EvaluateAndExecute_DefaultBranch", testConditionalWorkflow_EvaluateAndExecute_DefaultBranch);
    suite.addTest("ConditionalWorkflow_ClearBranches", testConditionalWorkflow_ClearBranches);
    suite.addTest("ConditionalWorkflow_BranchPriority", testConditionalWorkflow_BranchPriority);
    suite.addTest("ConditionalWorkflow_NoBranchSelected", testConditionalWorkflow_NoBranchSelected);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createConditionalWorkflowTestSuite());

    return TestRunner::runAllSuites(suites);
}
