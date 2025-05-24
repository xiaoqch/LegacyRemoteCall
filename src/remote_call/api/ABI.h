#pragma once

#include "ll/api/Expected.h"
#include "ll/api/mod/NativeMod.h"
#include "remote_call/api/value/DynamicValue.h"

namespace remote_call {

using CallbackFn = std::function<ll::Expected<DynamicValue>(std::vector<DynamicValue>&)>;

__declspec(dllexport) ll::Expected<> exportFunc(
    std::string_view            nameSpace,
    std::string_view            funcName,
    CallbackFn&&                callback,
    std::weak_ptr<ll::mod::Mod> mod = ll::mod::NativeMod::current()
);
__declspec(dllexport)      ll::Expected<std::reference_wrapper<CallbackFn>>
                           importFunc(std::string_view nameSpace, std::string_view funcName);
__declspec(dllexport) bool hasFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled = false);
__declspec(dllexport) std::weak_ptr<ll::mod::Mod> getProvider(std::string_view nameSpace, std::string_view funcName);
__declspec(dllexport) bool                        removeFunc(std::string_view nameSpace, std::string_view funcName);
__declspec(dllexport) int                         removeNameSpace(std::string_view nameSpace);
__declspec(dllexport) int removeFuncs(std::vector<std::pair<std::string, std::string>> const& funcs);

} // namespace remote_call