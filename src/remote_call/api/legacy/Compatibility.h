#include "ll/api/utils/SystemUtils.h"
#include "remote_call/api/value/DynamicValue.h"

// Keep old abi
namespace RemoteCall {
struct ValueType : remote_call::DynamicValue {};
using CallbackFn = std::function<ValueType(std::vector<ValueType>)>;

__declspec(dllexport) extern CallbackFn const EMPTY_FUNC;
__declspec(dllexport) bool                    exportFunc(
                       std::string const& nameSpace,
                       std::string const& funcName,
                       CallbackFn&&       callback,
                       void*              handle = ll::sys_utils::getCurrentModuleHandle()
                   );
__declspec(dllexport) CallbackFn const& importFunc(std::string const& nameSpace, std::string const& funcName);

__declspec(dllexport) bool hasFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllexport) bool removeFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllexport) int  removeNameSpace(std::string const& nameSpace);
__declspec(dllexport) int  removeFuncs(std::vector<std::pair<std::string, std::string>>& funcs);
__declspec(dllexport) void _onCallError(std::string const& msg, void* handle = ll::sys_utils::getCurrentModuleHandle());

} // namespace RemoteCall