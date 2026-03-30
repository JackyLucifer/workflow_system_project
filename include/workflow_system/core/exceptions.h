/**
 * @file exceptions.h
 * @brief 异常处理兼容层（向后兼容）
 *
 * 此文件提供向后兼容性，重定向到 common 库
 */

#ifndef WORKFLOW_SYSTEM_EXCEPTIONS_H
#define WORKFLOW_SYSTEM_EXCEPTIONS_H

#include "common/exceptions.h"

namespace WorkflowSystem {

// 导入异常基类
using Common::CommonException;

// 为了向后兼容，保留 WorkflowException 作为 CommonException 的别名
using WorkflowException = CommonException;

// 导入 StateException（使用 Common::StateException）
using Common::StateException;

// 向后兼容：保留 WorkflowStateException 别名
using WorkflowStateException = StateException;

// 导入其他异常类
using Common::TimeoutException;
using Common::ValidationException;
using Common::ResourceException;
using Common::ConfigurationException;

// 导入 ExceptionHandler
using Common::ExceptionHandler;

// 导入异常安全宏（重命名版本）
#define TRY_WF \
    try

#define CATCH_WF(context) \
    catch (const Common::CommonException& e) { \
        Common::ExceptionHandler::handle(e, context); \
    } catch (const std::exception& e) { \
        Common::ExceptionHandler::handle(e, context); \
    } catch (...) { \
        Common::ExceptionHandler::handleUnknown(context); \
    }

#define TRY_WF_OR_THROW \
    try

#define CATCH_WF_OR_THROW(context) \
    catch (const Common::CommonException& e) { \
        Common::ExceptionHandler::handle(e, context); \
        throw; \
    } catch (const std::exception& e) { \
        Common::ExceptionHandler::handle(e, context); \
        throw; \
    } catch (...) { \
        Common::ExceptionHandler::handleUnknown(context); \
        throw std::runtime_error(std::string("Unknown exception in ") + context); \
    }

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_EXCEPTIONS_H
