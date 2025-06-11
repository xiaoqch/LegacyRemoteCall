#pragma once

#include "ll/api/Expected.h"
#include "ll/api/mod/NativeMod.h"
#include "remote_call/api/base/Macro.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call {

using CallbackFn = std::function<ll::Expected<DynamicValue>(std::vector<DynamicValue>)>;


struct FunctionRef {
    CallbackFn const&                 callable;
    bool const                        returnExpected;
    bool const                        disabled;
    std::weak_ptr<ll::mod::Mod> const provider;
    // std::string_view                  nameSpace;
    // std::string_view                  functionName;

    [[nodiscard]] inline ll::Expected<DynamicValue> operator()(std::vector<DynamicValue>&& args) const {
        return callable(std::move(args));
    }
};

REMOTE_CALL_API ll::Expected<FunctionRef> exportFunc(
    std::string_view            nameSpace,
    std::string_view            funcName,
    CallbackFn&&                callback,
    bool                        returnExpected,
    std::weak_ptr<ll::mod::Mod> mod = ll::mod::NativeMod::current()
);
REMOTE_CALL_API      ll::Expected<FunctionRef>
                     importFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled = false);
REMOTE_CALL_API bool hasFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled = false);
REMOTE_CALL_API std::weak_ptr<ll::mod::Mod> getProvider(std::string_view nameSpace, std::string_view funcName);
REMOTE_CALL_API bool                        removeFunc(std::string_view nameSpace, std::string_view funcName);
REMOTE_CALL_API int                         removeNameSpace(std::string_view nameSpace);
REMOTE_CALL_API int                         removeFuncs(std::vector<std::pair<std::string, std::string>> const& funcs);

} // namespace remote_call