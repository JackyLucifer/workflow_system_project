/**
 * @file main.cpp
 * @brief Workflow System - Demo: Passing Objects to Workflow
 *
 * Complete Demo: Prepare Objects Outside -> Start Workflow -> Use Objects Inside -> Complete -> Cleanup
 */

#include <iostream>
#include <vector>
#include <memory>
#include "workflow_system/core/types.h"
#include "workflow_system/core/logger.h"
#include "workflow_system/core/utils.h"
#include "workflow_system/core/any.h"
#include "workflow_system/implementations/system_facade.h"
#include "workflow_system/implementations/workflows/base_workflow.h"

using namespace WorkflowSystem;

// ========== Custom Data Structures ==========

struct ProcessConfig {
    std::string mode;
    int maxRetries;
    double timeout;
    bool enableLogging;
};

struct UserData {
    std::string userId;
    std::string userName;
    int age;
    std::vector<std::string> tags;
};

struct ProcessingResult {
    std::string status;
    int processedCount;
    double averageTime;
    std::string summary;
};

// ========== Common Message Implementation ==========

class MyMessage : public IMessage {
public:
    MyMessage(const std::string& type, const std::string& topic, const std::string& payload)
        : type_(type), topic_(topic), payload_(payload) {}

    std::string getType() const override { return type_; }
    std::string getTopic() const override { return topic_; }
    std::string getPayload() const override { return payload_; }

private:
    std::string type_;
    std::string topic_;
    std::string payload_;
};

// ========== Custom Workflow - Demo Object Passing and Usage ==========

class DataProcessingWorkflow : public BaseWorkflow {
public:
    DataProcessingWorkflow(std::shared_ptr<IResourceManager> resourceManager)
        : BaseWorkflow("DataProcessingWorkflow", resourceManager),
          executionStartTime_(0), executionEndTime_(0) {}

    // Get processing result
    ProcessingResult getLastResult() const { return lastResult_; }

    // Get execution time (in milliseconds)
    long getExecutionTimeMs() const {
        return executionEndTime_ - executionStartTime_;
    }

protected:
    void onStart() override {
        LOG_INFO("[DataProcessingWorkflow] ========== Workflow Started ==========");

        auto ctx = getContext();

        // ========== Read Passed Objects ==========
        std::cout << std::endl << "[Workflow Startup] Reading passed objects..." << std::endl;

        // 1. Read user data
        try {
            auto userData = ctx->getObject<UserData>("user_data");
            std::cout << "  [OK] User Data: " << userData.userName
                      << " (ID: " << userData.userId
                      << ", Age: " << userData.age
                      << ", Tags: ";
            for (const auto& tag : userData.tags) {
                std::cout << tag << " ";
            }
            std::cout << ")" << std::endl;
        } catch (...) {
            std::cout << "  [X] User Data: Not set" << std::endl;
        }

        // 2. Read process config
        try {
            auto config = ctx->getObject<ProcessConfig>("config");
            std::cout << "  [OK] Config: Mode=" << config.mode
                      << ", Retries=" << config.maxRetries
                      << ", Timeout=" << config.timeout
                      << ", Logging=" << (config.enableLogging ? "ON" : "OFF") << std::endl;
        } catch (...) {
            std::cout << "  [X] Config: Not set, using defaults" << std::endl;
        }

        // 3. Read input data
        try {
            auto inputData = ctx->getObject<std::vector<int>>("input_data");
            std::cout << "  [OK] Input Data: [" << inputData.size() << " numbers] = ";
            for (int num : inputData) {
                std::cout << num << " ";
            }
            std::cout << std::endl;
        } catch (...) {
            std::cout << "  [X] Input Data: Not set, using empty data" << std::endl;
        }
    }

    void onMessage(const IMessage& message) override {
        LOG_INFO("[DataProcessingWorkflow] Received message: " + message.getTopic());

        auto ctx = getContext();
        auto topic = message.getTopic();

        if (topic == "process") {
            std::cout << std::endl << "[Message Handler] Starting data processing..." << std::endl;

            auto config = ctx->getObject<ProcessConfig>("config");
            ProcessConfig actualConfig;
            try {
                actualConfig = ctx->getObject<ProcessConfig>("config");
            } catch (...) {
                actualConfig.mode = "normal";
                actualConfig.maxRetries = 3;
                actualConfig.timeout = 1.0;
                actualConfig.enableLogging = true;
            }

            auto inputData = ctx->getObject<std::vector<int>>("input_data");
            std::vector<int> actualInputData;
            try {
                actualInputData = ctx->getObject<std::vector<int>>("input_data");
            } catch (...) {
                actualInputData = {1, 2, 3};
            }

            // Simulate data processing
            ProcessingResult result;
            result.status = "processing";
            result.processedCount = actualInputData.size();

            double sum = 0;
            for (int num : actualInputData) {
                sum += num;
            }
            result.averageTime = sum / actualInputData.size();

            // Simulate processing delay
            if (actualConfig.enableLogging) {
                std::cout << "  - Processing " << result.processedCount << " items" << std::endl;
            }

            result.status = "completed";
            result.summary = "Processing completed, total " + std::to_string(result.processedCount) + " items";

            // Store result
            ctx->setObject("result", result);
            std::cout << "  [OK] Processing completed!" << std::endl;
        }
        else if (topic == "get_result") {
            try {
                auto result = ctx->getObject<ProcessingResult>("result");
                std::cout << std::endl << "[Result Query]" << std::endl;
                std::cout << "  Status: " << result.status << std::endl;
                std::cout << "  Processed Count: " << result.processedCount << std::endl;
                std::cout << "  Average Time: " << result.averageTime << "ms" << std::endl;
                std::cout << "  Summary: " << result.summary << std::endl;

                lastResult_ = result;
            } catch (...) {
                std::cout << "  [X] Result not generated yet" << std::endl;
            }
        }
    }

    void onInterrupt() override {
        LOG_WARNING("[DataProcessingWorkflow] Workflow Interrupted");
        std::cout << "  [!] Workflow interrupted" << std::endl;
    }

    void onFinalize() override {
        LOG_INFO("[DataProcessingWorkflow] Workflow Finished");
        std::cout << std::endl << "[Workflow Completion] Cleaning up context..." << std::endl;

        auto ctx = getContext();

        // Cleanup context (auto cleanup working directory)
        ctx->cleanup();

        std::cout << "  [OK] Cleanup completed!" << std::endl;
    }

    // Execute workflow main function (runs in separate thread)
    Any onExecute() override {
        auto ctx = getContext();

        // Record start time
        executionStartTime_ = TimeUtils::getCurrentMs();

        // Simulate data processing and return result
        ProcessingResult result;

        auto config = ctx->getObject<ProcessConfig>("config");
        ProcessConfig actualConfig;
        try {
            actualConfig = ctx->getObject<ProcessConfig>("config");
        } catch (...) {
            actualConfig.mode = "normal";
            actualConfig.maxRetries = 3;
            actualConfig.timeout = 1.0;
            actualConfig.enableLogging = true;
        }

        auto inputData = ctx->getObject<std::vector<int>>("input_data");
        std::vector<int> actualInputData;
        try {
            actualInputData = ctx->getObject<std::vector<int>>("input_data");
        } catch (...) {
            actualInputData = {1, 2, 3};
        }

        std::cout << std::endl << "[Execution] Processing data..." << std::endl;

        // Simulate data processing
        result.status = "processing";
        result.processedCount = actualInputData.size();

        double sum = 0;
        for (int num : actualInputData) {
            sum += num;
        }
        result.averageTime = sum / actualInputData.size();

        // Simulate processing delay
        if (actualConfig.enableLogging) {
            std::cout << "  - Processing " << result.processedCount << " items" << std::endl;
        }

        result.status = "completed";
        result.summary = "Processing completed, total " + std::to_string(result.processedCount) + " items";

        // Store result in context
        ctx->setObject("result", result);
        std::cout << "  [OK] Processing completed!" << std::endl;

        // Save result to member variable
        lastResult_ = result;

        // Record end time
        executionEndTime_ = TimeUtils::getCurrentMs();

        // Print execution time
        long executionTimeMs = executionEndTime_ - executionStartTime_;
        std::cout << "  [OK] Execution time: " << executionTimeMs << " ms" << std::endl;

        // Return result
        return Any(result);
    }

private:
    ProcessingResult lastResult_;
    long executionStartTime_;
    long executionEndTime_;
};

// ========== Convenient Print Functions ==========

void printHeader(const std::string& title) {
    std::cout << "========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

void printSection(const std::string& title) {
    std::cout << std::endl;
    std::cout << ">>> " << title << std::endl;
    std::cout << std::endl;
}

int main() {
    printHeader("Workflow System - Complete Object Passing Demo");

    // ========== Step 1: Initialize System ==========
    printSection("Step 1: Initialize System");
    auto& system = SystemFacade::getInstance();
    system.initialize("/tmp/workflow_demo");
    auto manager = system.getManager();
    std::cout << "  [OK] System initialized" << std::endl;

    // ========== Step 2: Register Workflow ==========
    printSection("Step 2: Register Workflow");
    auto workflow = std::make_shared<DataProcessingWorkflow>(
        manager->getResourceManager()
    );
    manager->registerWorkflow("DataProcessingWorkflow",
        std::static_pointer_cast<IWorkflow>(workflow));
    std::cout << "  [OK] Workflow registered" << std::endl;

    // ========== Step 3: Prepare Objects to Pass ==========
    printSection("Step 3: Prepare Objects to Pass");

    // 3.1 User data
    UserData user;
    user.userId = "U20240312";
    user.userName = "Zhang San";
    user.age = 30;
    user.tags = {"admin", "developer", "tester"};

    std::cout << "Objects to pass:" << std::endl;
    std::cout << "  1. UserData (User Info)" << std::endl;
    std::cout << "     Name: " << user.userName << std::endl;
    std::cout << "     ID: " << user.userId << std::endl;
    std::cout << "     Age: " << user.age << std::endl;
    std::cout << "     Tags: ";
    for (const auto& tag : user.tags) {
        std::cout << tag << " ";
    }
    std::cout << std::endl << std::endl;

    // 3.2 Process Config
    ProcessConfig config;
    config.mode = "fast";
    config.maxRetries = 5;
    config.timeout = 2.5;
    config.enableLogging = true;

    std::cout << "  2. ProcessConfig (Process Config)" << std::endl;
    std::cout << "     Mode: " << config.mode << std::endl;
    std::cout << "     Max Retries: " << config.maxRetries << std::endl;
    std::cout << "     Timeout: " << config.timeout << "s" << std::endl;
    std::cout << "     Logging: " << (config.enableLogging ? "ON" : "OFF") << std::endl << std::endl;

    // 3.3 Input data
    std::vector<int> inputData = {10, 25, 30, 42, 55};

    std::cout << "  3. Input Data (std::vector)" << std::endl;
    std::cout << "     Data: [10, 25, 30, 42, 55]" << std::endl;
    std::cout << "     Count: " << inputData.size() << std::endl << std::endl;

    // ========== Step 4: Start Workflow with Passed Objects ==========
    printSection("Step 4: Start Workflow with Passed Objects");

    // Create context (outside workflow)
    auto resourceManager = manager->getResourceManager();
    auto ctx = std::make_shared<WorkflowContext>(resourceManager);

    // Set objects to context (outside workflow)
    std::cout << "Setting objects to context..." << std::endl;
    ctx->setObject("user_data", user);
    ctx->setObject("config", config);
    ctx->setObject("input_data", inputData);
    std::cout << "  [OK] All objects set to context" << std::endl;

    // Start workflow with pre-configured context
    manager->startWorkflow("DataProcessingWorkflow", ctx);

    // ========== Step 5: Execute Workflow and Get Result ==========
    printSection("Step 5: Execute Workflow and Get Result");

    // Call execute() method in separate thread
    std::cout << "Starting workflow execution..." << std::endl;
    auto future = workflow->execute();

    // Wait for execution completion and get result
    std::cout << "Waiting for execution completion..." << std::endl;
    auto result = future.get();

    // Display execution result
    if (result.has_value()) {
        auto processingResult = result.cast<ProcessingResult>();
        std::cout << std::endl << "[Execution Result]" << std::endl;
        std::cout << "  Status: " << processingResult.status << std::endl;
        std::cout << "  Processed Count: " << processingResult.processedCount << std::endl;
        std::cout << "  Average Time: " << processingResult.averageTime << "ms" << std::endl;
        std::cout << "  Summary: " << processingResult.summary << std::endl;

        std::cout << std::endl << "Workflow State: " << workflowStateToString(workflow->getState()) << std::endl;
        std::cout << "  Execution Time: " << workflow->getExecutionTimeMs() << " ms" << std::endl;
    } else {
        std::cout << std::endl << "[Execution Result] Execution failed, no result" << std::endl;
    }
    std::cout << "  [OK] Execution completed!" << std::endl;

    // ========== Step 6: Display Final Status ==========
    printSection("Step 6: Display Final Status");
    std::cout << "System Status:" << std::endl;
    std::cout << manager->getStatus() << std::endl;

    // ========== Demo Complete ==========
    printHeader("Demo Complete");

    // ========== Cleanup ==========
    std::cout << "Shutting down system..." << std::endl;
    system.shutdown();
    std::cout << "  [OK] System shutdown" << std::endl;

    return 0;
}
