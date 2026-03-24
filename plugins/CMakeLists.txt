cmake_minimum_required(VERSION 3.10)
project(WorkflowPluginSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -fPIC)
endif()

# 目录设置
set(PLUGIN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PLUGIN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PLUGIN_EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/plugins)

# 输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ========== 核心库（共享库）==========
add_library(plugin_core SHARED
    ${PLUGIN_SRC_DIR}/plugin_loader.cpp
)

target_include_directories(plugin_core PUBLIC
    ${PLUGIN_INCLUDE_DIR}
)

target_link_libraries(plugin_core PRIVATE
    ${CMAKE_DL_LIBS}
)

# 设置符号可见性
set_target_properties(plugin_core PROPERTIES
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)

# ========== 插件接口头文件库（header-only）==========
add_library(plugin_interface INTERFACE)
target_include_directories(plugin_interface INTERFACE
    ${PLUGIN_INCLUDE_DIR}
)

# ========== 示例插件 ==========
# Hello 插件
add_library(hello_plugin SHARED
    ${PLUGIN_EXAMPLES_DIR}/hello_plugin/hello_plugin.cpp
)

target_include_directories(hello_plugin PRIVATE
    ${PLUGIN_INCLUDE_DIR}
)

target_link_libraries(hello_plugin PRIVATE
    plugin_interface
)

set_target_properties(hello_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "hello_plugin"
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
)

# Advanced 插件
add_library(advanced_plugin SHARED
    ${PLUGIN_EXAMPLES_DIR}/advanced_plugin/advanced_plugin.cpp
)

target_include_directories(advanced_plugin PRIVATE
    ${PLUGIN_INCLUDE_DIR}
)

target_link_libraries(advanced_plugin PRIVATE
    plugin_interface
)

set_target_properties(advanced_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "advanced_plugin"
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
)

# ========== 测试宿主程序 ==========
add_executable(test_plugin_host
    ${PLUGIN_EXAMPLES_DIR}/test_plugin_host.cpp
)

target_include_directories(test_plugin_host PRIVATE
    ${PLUGIN_INCLUDE_DIR}
)

target_link_libraries(test_plugin_host PRIVATE
    plugin_core
)

# 重要：导出符号供动态库使用
set_target_properties(test_plugin_host PROPERTIES
    ENABLE_EXPORTS ON
)

if(NOT WIN32)
    target_link_options(test_plugin_host PRIVATE -rdynamic)
endif()

# ========== 单元测试 ==========
add_executable(plugin_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/plugin_tests.cpp
)

target_include_directories(plugin_tests PRIVATE
    ${PLUGIN_INCLUDE_DIR}
)

target_link_libraries(plugin_tests PRIVATE
    plugin_core
)

# ========== 安装规则 ==========
install(TARGETS plugin_core plugin_interface
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${PLUGIN_INCLUDE_DIR}/
    DESTINATION include/workflow_plugin
)

# ========== 消息 ==========
message(STATUS "")
message(STATUS "WorkflowPluginSystem 配置:")
message(STATUS "  版本: ${PROJECT_VERSION}")
message(STATUS "  C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  插件输出目录: ${PLUGIN_OUTPUT_DIR}")
message(STATUS "  示例插件: hello_plugin")
message(STATUS "")
