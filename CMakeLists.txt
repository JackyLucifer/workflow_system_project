cmake_minimum_required(VERSION 3.10)
project(WorkflowSystem VERSION 1.0 LANGUAGES CXX)

# C++ 标准（使用 C++11）
set(CMAKE_CXX_STANDARD 11)
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

# ========== 输出目录配置 ==========
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ========== 添加公共库 ==========
add_subdirectory(${CMAKE_SOURCE_DIR}/../common ${CMAKE_BINARY_DIR}/common)

# ========== 收集源文件 ==========
# 排除已移到 common/ 的文件
file(GLOB_RECURSE ALL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/implementations/*.cpp
)

# 过滤掉已经移到 common 库的源文件
list(FILTER ALL_SOURCES EXCLUDE REGEX ".*/async_logger\\.cpp$")
list(FILTER ALL_SOURCES EXCLUDE REGEX ".*/json_config_manager\\.cpp$")

set(WORKFLOW_SOURCES ${ALL_SOURCES})

# ========== 收集插件系统源文件 ==========
file(GLOB_RECURSE PLUGIN_SOURCES
    ${CMAKE_SOURCE_DIR}/src/plugin/*.cpp
)

# ========== 创建静态库 ==========
add_library(workflow_system_static STATIC ${WORKFLOW_SOURCES})
target_include_directories(workflow_system_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/../common/include>
    $<INSTALL_INTERFACE:include>
)

# 链接必要的依赖
target_link_libraries(workflow_system_static PUBLIC
    Threads::Threads
    common_static
)

# 如果找到 SQLite3，添加链接
if(SQLITE3_FOUND)
    target_link_libraries(workflow_system_static PUBLIC ${SQLITE3_LIBRARIES})
    target_include_directories(workflow_system_static PRIVATE ${SQLITE3_INCLUDE_DIRS})
endif()

# ========== 插件框架核心库 ==========
add_library(plugin_framework_core SHARED ${PLUGIN_SOURCES})

target_include_directories(plugin_framework_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/../common/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(plugin_framework_core PUBLIC
    Threads::Threads
    ${CMAKE_DL_LIBS}
    common_static
)

# 设置输出名称和版本
set_target_properties(plugin_framework_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# 创建别名库
add_library(workflow_system::plugin ALIAS plugin_framework_core)

# ========== 示例插件 ==========

# 计算器插件
add_library(calculator_plugin SHARED
    plugins/examples/CalculatorPlugin.cpp
)

target_include_directories(calculator_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(calculator_plugin PRIVATE
    plugin_framework_core
)

set_target_properties(calculator_plugin PROPERTIES
    OUTPUT_NAME "calculator"
    PREFIX ""
    SUFFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# 演示插件
add_library(demo_plugin SHARED
    plugins/examples/DemoPlugin.cpp
)

target_include_directories(demo_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(demo_plugin PRIVATE
    plugin_framework_core
)

set_target_properties(demo_plugin PROPERTIES
    OUTPUT_NAME "demo_plugin"
    PREFIX "lib"
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# ========== 插件示例程序 ==========

add_executable(plugin_host examples/plugin/host_main.cpp)
target_link_libraries(plugin_host PRIVATE plugin_framework_core)

# ========== 插件系统测试 ==========

add_executable(test_channel tests/plugin/test_channel.cpp)
target_include_directories(test_channel PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_channel PRIVATE Threads::Threads)

add_executable(test_service_locator tests/plugin/test_service_locator.cpp)
target_include_directories(test_service_locator PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_service_locator PRIVATE Threads::Threads)

# 创建别名库，方便使用
add_library(workflow_system::system ALIAS workflow_system_static)

# ========== 辅助函数：创建示例 ==========
function(add_workflow_example name)
    add_executable(${name} examples/${name}.cpp)
    target_link_libraries(${name} PRIVATE workflow_system_static)

    # 如果需要 SQLite3
    if(SQLITE3_FOUND)
        target_link_libraries(${name} PRIVATE ${SQLITE3_LIBRARIES})
        target_include_directories(${name} PRIVATE ${SQLITE3_INCLUDE_DIRS})
    endif()
endfunction()

# ========== 辅助函数：创建测试 ==========
function(add_workflow_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE workflow_system_static)

    # 添加测试头文件目录
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/tests)

    # 如果需要 SQLite3
    if(SQLITE3_FOUND)
        target_link_libraries(${name} PRIVATE ${SQLITE3_LIBRARIES})
        target_include_directories(${name} PRIVATE ${SQLITE3_INCLUDE_DIRS})
    endif()
endfunction()

# ========== 创建示例可执行文件 ==========
add_workflow_example(main)
add_workflow_example(queue_example)
add_workflow_example(advanced_example)
add_workflow_example(config_example)
add_workflow_example(orchestration_example)
add_workflow_example(observer_example)
add_workflow_example(comprehensive_example)
add_workflow_example(persistence_example)
add_workflow_example(version_control_example)
add_workflow_example(conditional_example)
add_workflow_example(workflow_template_example)
add_workflow_example(scheduler_example)
add_workflow_example(checkpoint_example)


# ========== 创建测试可执行文件 ==========
add_workflow_test(test_all)
add_workflow_test(test_core_types)
add_workflow_test(test_workflow_graph)
add_workflow_test(test_config_manager)
add_workflow_test(test_extended_modules)
add_workflow_test(test_workflow_orchestrator)
add_workflow_test(test_workflow_persistence)
add_workflow_test(test_workflow_version_control)
add_workflow_test(test_workflow_template)
add_workflow_test(test_conditional_workflow)
add_workflow_test(test_workflow_observer)
add_workflow_test(test_workflow_manager)
add_workflow_test(test_memory_pool)
add_workflow_test(test_async_logger)
add_workflow_test(test_system_facade)

# ========== 测试快捷目标 ==========
add_custom_target(run_tests
    COMMAND ${CMAKE_BINARY_DIR}/bin/test_all
    DEPENDS test_all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行所有单元测试..."
)

add_custom_target(test
    COMMAND ${CMAKE_BINARY_DIR}/bin/test_all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行基础测试..."
)

# ========== 安装规则 ==========
install(TARGETS workflow_system_static plugin_framework_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# 安装示例插件
install(TARGETS calculator_plugin demo_plugin
    LIBRARY DESTINATION lib/workflow_system/plugins
    RUNTIME DESTINATION lib/workflow_system/plugins
)
