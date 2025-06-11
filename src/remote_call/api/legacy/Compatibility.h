#pragma once

#include "ll/api/utils/SystemUtils.h"
#include "remote_call/api/base/Macro.h"
#include "remote_call/api/value/DynamicValue.h"

// Keep old abi
namespace RemoteCall {
struct ValueType : remote_call::DynamicValue {};
using CallbackFn = std::function<ValueType(std::vector<ValueType>)>;

REMOTE_CALL_API extern CallbackFn const EMPTY_FUNC;
REMOTE_CALL_API bool                    exportFunc(
                       std::string const& nameSpace,
                       std::string const& funcName,
                       CallbackFn&&       callback,
                       void*              handle = ll::sys_utils::getCurrentModuleHandle()
                   );
REMOTE_CALL_API CallbackFn const& importFunc(std::string const& nameSpace, std::string const& funcName);

REMOTE_CALL_API bool hasFunc(std::string const& nameSpace, std::string const& funcName);
REMOTE_CALL_API bool removeFunc(std::string const& nameSpace, std::string const& funcName);
REMOTE_CALL_API int  removeNameSpace(std::string const& nameSpace);
REMOTE_CALL_API int  removeFuncs(std::vector<std::pair<std::string, std::string>>& funcs);
REMOTE_CALL_API void _onCallError(std::string const& msg, void* handle = ll::sys_utils::getCurrentModuleHandle());

} // namespace RemoteCall