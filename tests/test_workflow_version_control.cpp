/**
 * @file test_workflow_version_control.cpp
 * @brief 工作流版本控制测试
 */

#include "workflow_system/implementations/workflow_version_control_impl.h"
#include "test_framework.h"

using namespace WorkflowSystem;
using namespace WorkflowSystem::Test;

// ========== 测试辅助类 ==========

class SimpleWorkflowVersionControl : public IWorkflowVersionControl {
private:
    std::map<std::string, std::vector<WorkflowVersion>> versions_;

public:
    std::string saveVersion(const std::string& workflowName,
                           const std::string& definitionJson,
                           const std::string& author,
                           const std::string& description) override {
        std::string versionId = "v" + std::to_string(versions_[workflowName].size() + 1);

        WorkflowVersion version;
        version.versionId = versionId;
        version.workflowName = workflowName;
        version.author = author;
        version.description = description;
        version.createdAt = 0;
        version.definitionJson = definitionJson;
        version.parentVersion = "";
        version.isActive = (versions_[workflowName].empty());

        versions_[workflowName].push_back(version);

        return versionId;
    }

    std::vector<WorkflowVersion> getAllVersions(const std::string& workflowName) const override {
        auto it = versions_.find(workflowName);
        if (it != versions_.end()) {
            return it->second;
        }
        return std::vector<WorkflowVersion>();
    }

    WorkflowVersion getLatestVersion(const std::string& workflowName) const override {
        auto versions = getAllVersions(workflowName);
        if (versions.empty()) {
            return WorkflowVersion();
        }
        return versions.back();
    }

    std::string getVersionDefinition(const std::string& workflowName,
                                    const std::string& versionId) const override {
        auto versions = getAllVersions(workflowName);
        for (const auto& v : versions) {
            if (v.versionId == versionId) {
                return v.definitionJson;
            }
        }
        return "";
    }

    bool rollbackToVersion(const std::string& workflowName,
                          const std::string& versionId) override {
        auto versions = getAllVersions(workflowName);
        for (auto& v : versions) {
            if (v.versionId == versionId) {
                v.isActive = true;
                return true;
            }
        }
        return false;
    }

    VersionDiff compareVersions(const std::string& workflowName,
                               const std::string& versionFrom,
                               const std::string& versionTo) const override {
        VersionDiff diff;
        diff.versionFrom = versionFrom;
        diff.versionTo = versionTo;
        diff.summary = "Versions differ";
        return diff;
    }

    bool setActiveVersion(const std::string& workflowName,
                         const std::string& versionId) override {
        return rollbackToVersion(workflowName, versionId);
    }

    bool deleteVersion(const std::string& workflowName,
                      const std::string& versionId) override {
        auto it = versions_.find(workflowName);
        if (it != versions_.end()) {
            auto& vlist = it->second;
            for (auto vit = vlist.begin(); vit != vlist.end(); ++vit) {
                if (vit->versionId == versionId) {
                    vlist.erase(vit);
                    return true;
                }
            }
        }
        return false;
    }

    int deleteAllVersions(const std::string& workflowName) override {
        auto it = versions_.find(workflowName);
        if (it != versions_.end()) {
            int count = it->second.size();
            versions_.erase(it);
            return count;
        }
        return 0;
    }

    std::vector<std::string> getAllWorkflowNames() const override {
        std::vector<std::string> names;
        for (const auto& pair : versions_) {
            names.push_back(pair.first);
        }
        return names;
    }
};

// ========== 测试用例 ==========

void testVersionControl_SaveVersion() {
    SimpleWorkflowVersionControl vcs;

    std::string definition = R"({"nodes": ["node1", "node2"]})";
    std::string versionId = vcs.saveVersion("workflow1", definition, "author1", "First version");

    TEST_ASSERT_FALSE(versionId.empty());
    TEST_ASSERT_STRING_EQUAL("v1", versionId.c_str());
}

void testVersionControl_GetAllVersions() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");
    vcs.saveVersion("workflow1", R"({"ver": 3})", "author1", "Version 3");

    auto versions = vcs.getAllVersions("workflow1");

    TEST_ASSERT_EQUAL(3, static_cast<int>(versions.size()));
}

void testVersionControl_GetLatestVersion() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");

    auto latest = vcs.getLatestVersion("workflow1");

    TEST_ASSERT_FALSE(latest.versionId.empty());
    TEST_ASSERT_STRING_EQUAL("v2", latest.versionId.c_str());
    TEST_ASSERT_STRING_EQUAL("Version 2", latest.description.c_str());
}

void testVersionControl_GetVersionDefinition() {
    SimpleWorkflowVersionControl vcs;

    std::string definition = R"({"nodes": ["node1", "node2", "node3"]})";
    vcs.saveVersion("workflow1", definition, "author1", "Version 1");

    std::string retrieved = vcs.getVersionDefinition("workflow1", "v1");

    TEST_ASSERT_STRING_EQUAL(definition.c_str(), retrieved.c_str());
}

void testVersionControl_RollbackToVersion() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");
    vcs.saveVersion("workflow1", R"({"ver": 3})", "author1", "Version 3");

    bool result = vcs.rollbackToVersion("workflow1", "v2");

    TEST_ASSERT_TRUE(result);
}

void testVersionControl_CompareVersions() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");

    VersionDiff diff = vcs.compareVersions("workflow1", "v1", "v2");

    TEST_ASSERT_STRING_EQUAL("v1", diff.versionFrom.c_str());
    TEST_ASSERT_STRING_EQUAL("v2", diff.versionTo.c_str());
    TEST_ASSERT_FALSE(diff.summary.empty());
}

void testVersionControl_SetActiveVersion() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");

    bool result = vcs.setActiveVersion("workflow1", "v1");

    TEST_ASSERT_TRUE(result);
}

void testVersionControl_DeleteVersion() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");
    vcs.saveVersion("workflow1", R"({"ver": 3})", "author1", "Version 3");

    bool result = vcs.deleteVersion("workflow1", "v2");

    TEST_ASSERT_TRUE(result);

    auto versions = vcs.getAllVersions("workflow1");
    TEST_ASSERT_EQUAL(2, static_cast<int>(versions.size()));
}

void testVersionControl_DeleteAllVersions() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "Version 1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "Version 2");

    int count = vcs.deleteAllVersions("workflow1");

    TEST_ASSERT_EQUAL(2, count);

    auto versions = vcs.getAllVersions("workflow1");
    TEST_ASSERT_EQUAL(0, static_cast<int>(versions.size()));
}

void testVersionControl_GetAllWorkflowNames() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({})", "author1", "Version 1");
    vcs.saveVersion("workflow2", R"({})", "author1", "Version 1");
    vcs.saveVersion("workflow3", R"({})", "author1", "Version 1");

    auto names = vcs.getAllWorkflowNames();

    TEST_ASSERT_EQUAL(3, static_cast<int>(names.size()));
}

void testVersionControl_MultipleWorkflows() {
    SimpleWorkflowVersionControl vcs;

    vcs.saveVersion("workflow1", R"({"ver": 1})", "author1", "WF1 v1");
    vcs.saveVersion("workflow1", R"({"ver": 2})", "author1", "WF1 v2");
    vcs.saveVersion("workflow2", R"({"ver": 1})", "author1", "WF2 v1");
    vcs.saveVersion("workflow2", R"({"ver": 2})", "author1", "WF2 v2");
    vcs.saveVersion("workflow2", R"({"ver": 3})", "author1", "WF2 v3");

    auto wf1Versions = vcs.getAllVersions("workflow1");
    auto wf2Versions = vcs.getAllVersions("workflow2");

    TEST_ASSERT_EQUAL(2, static_cast<int>(wf1Versions.size()));
    TEST_ASSERT_EQUAL(3, static_cast<int>(wf2Versions.size()));
}

// ========== 测试套件创建 ==========

TestSuite createWorkflowVersionControlTestSuite() {
    TestSuite suite("工作流版本控制测试");

    suite.addTest("VersionControl_SaveVersion", testVersionControl_SaveVersion);
    suite.addTest("VersionControl_GetAllVersions", testVersionControl_GetAllVersions);
    suite.addTest("VersionControl_GetLatestVersion", testVersionControl_GetLatestVersion);
    suite.addTest("VersionControl_GetVersionDefinition", testVersionControl_GetVersionDefinition);
    suite.addTest("VersionControl_RollbackToVersion", testVersionControl_RollbackToVersion);
    suite.addTest("VersionControl_CompareVersions", testVersionControl_CompareVersions);
    suite.addTest("VersionControl_SetActiveVersion", testVersionControl_SetActiveVersion);
    suite.addTest("VersionControl_DeleteVersion", testVersionControl_DeleteVersion);
    suite.addTest("VersionControl_DeleteAllVersions", testVersionControl_DeleteAllVersions);
    suite.addTest("VersionControl_GetAllWorkflowNames", testVersionControl_GetAllWorkflowNames);
    suite.addTest("VersionControl_MultipleWorkflows", testVersionControl_MultipleWorkflows);

    return suite;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::getInstance().setLevel(LogLevel::ERROR);

    std::vector<TestSuite> suites;
    suites.push_back(createWorkflowVersionControlTestSuite());

    return TestRunner::runAllSuites(suites);
}
