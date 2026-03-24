cmake_minimum_required(VERSION 3.10)
project(WorkflowSystem VERSION 1.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 14)  // 使用 C++14 以支持 std::shared_mutex
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 线程库
find_package(Threads REQUIRED)

# SQLite3 库 - 使用 pkg-config 查找
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
endif()

# ========== 头文件目录 ==========
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/implementations)

# ========== 输出目录配置 ==========
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ========== 创建静态库（头文件库）==========

# 创建 INTERFACE 库
add_library(workflow_system INTERFACE)
target_include_directories(workflow_system INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 链接必要的依赖
target_link_libraries(workflow_system INTERFACE
    Threads::Threads
)

# ========== 创建示例可执行文件 ==========

# 基础示例
add_executable(workflow_example examples/main.cpp)
target_link_libraries(workflow_example PRIVATE workflow_system)

add_executable(queue_example examples/queue_example.cpp)
target_link_libraries(queue_example PRIVATE workflow_system)

add_executable(advanced_example examples/advanced_example.cpp)
target_link_libraries(advanced_example PRIVATE workflow_system)

# 需要JSON配置的示例
add_executable(config_example examples/config_example.cpp src/implementations/json_config_manager.cpp)
target_link_libraries(config_example PRIVATE workflow_system Threads::Threads)

add_executable(orchestration_example examples/orchestration_example.cpp src/implementations/json_config_manager.cpp)
target_link_libraries(orchestration_example PRIVATE workflow_system Threads::Threads)

add_executable(observer_example examples/observer_example.cpp src/implementations/json_config_manager.cpp)
target_link_libraries(observer_example PRIVATE workflow_system Threads::Threads)

add_executable(comprehensive_example examples/comprehensive_example.cpp src/implementations/json_config_manager.cpp)
target_link_libraries(comprehensive_example PRIVATE workflow_system Threads::Threads)

# 需要SQLite的示例
add_executable(persistence_example examples/persistence_example.cpp src/implementations/sqlite_workflow_persistence.cpp)
target_link_libraries(persistence_example PRIVATE workflow_system Threads::Threads ${SQLITE3_LIBRARIES})
target_include_directories(persistence_example PRIVATE ${SQLITE3_INCLUDE_DIRS})

add_executable(version_control_example examples/version_control_example.cpp src/implementations/sqlite_workflow_persistence.cpp)
target_link_libraries(version_control_example PRIVATE workflow_system Threads::Threads ${SQLITE3_LIBRARIES})
target_include_directories(version_control_example PRIVATE ${SQLITE3_INCLUDE_DIRS})

# 其他示例
add_executable(conditional_example examples/conditional_example.cpp)
target_link_libraries(conditional_example PRIVATE workflow_system Threads::Threads)

add_executable(workflow_template_example examples/workflow_template_example.cpp)
target_link_libraries(workflow_template_example PRIVATE workflow_system Threads::Threads)

add_executable(scheduler_example examples/scheduler_example.cpp)
target_link_libraries(scheduler_example PRIVATE workflow_system Threads::Threads)

# 检查点管理器示例
add_executable(checkpoint_example examples/checkpoint_example.cpp src/implementations/checkpoint_manager_impl.cpp)
target_link_libraries(checkpoint_example PRIVATE workflow_system Threads::Threads)

# 性能优化示例
add_executable(performance_optimization_example examples/performance_optimization_example.cpp)
target_link_libraries(performance_optimization_example PRIVATE workflow_system Threads::Threads)

# 重构功能示例
add_executable(refactoring_examples examples/refactoring_examples.cpp)
target_link_libraries(refactoring_examples PRIVATE workflow_system Threads::Threads)

# ========== 测试可执行文件 ==========

add_executable(test_all tests/test_all.cpp)
target_include_directories(test_all PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_all PRIVATE workflow_system Threads::Threads)

add_executable(test_core_types tests/test_core_types.cpp)
target_include_directories(test_core_types PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_core_types PRIVATE workflow_system Threads::Threads)

# 测试快捷目标
add_custom_target(run_tests
    COMMAND ${CMAKE_BINARY_DIR}/bin/test_all
    DEPENDS test_all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行所有单元测试..."
)

add_custom_target(test
    COMMAND ${CMAKE_BINARY_DIR}/bin/test_all && ${CMAKE_BINARY_DIR}/bin/test_core_types
    DEPENDS test_all test_core_types
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行所有测试..."
)

# ========== 安装规则 ==========

# 安装头文件
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# 安装示例程序（可选）
# install(TARGETS workflow_example queue_example advanced_example
#     config_example orchestration_example observer_example
#     comprehensive_example persistence_example version_control_example
#     conditional_example workflow_template_example scheduler_example
#     RUNTIME DESTINATION bin/examples
# )

# ========== 开发模式辅助 ==========

# 如果需要调试，可以添加以下选项
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
# set(CMAKE_BUILD_TYPE Debug)
