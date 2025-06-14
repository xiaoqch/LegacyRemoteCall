#include "ABI.h"

#include "fmt/core.h"
#include "ll/api/Expected.h"
#include "ll/api/mod/Mod.h"
#include "ll/api/mod/ModManagerRegistry.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/utils/IdentifierMap.h"


namespace remote_call {

using CallableType = CallbackFn;

struct ExportedFuncData {
    CallableType                callable;
    std::weak_ptr<ll::mod::Mod> provider;
    bool                        returnExpected;
    bool                        disabled = false;
    // NOLINTNEXTLINE: google-explicit-constructor
    inline operator FunctionRef() { return {callable, returnExpected, disabled, provider}; }
};


IdentifierMap<ExportedFuncData> exportedFuncs;

void clearMod(std::string_view name) {
    for (auto iter = exportedFuncs.begin(); iter != exportedFuncs.end();) {
        auto mod = iter->second.provider.lock();
        if (!mod || mod->getName() == name) {
            iter = exportedFuncs.erase(iter);
        } else ++iter;
    }
}

void switchMod(std::string_view name, bool enable) {
    for (auto iter = exportedFuncs.begin(); iter != exportedFuncs.end();) {
        auto mod = iter->second.provider.lock();
        if (!mod) {
            iter = exportedFuncs.erase(iter);
        } else {
            if (mod->getName() == name) {
                iter->second.disabled = !enable;
            }
            ++iter;
        };
    }
}

bool registerOnModUnload() {
    auto& reg = ll::mod::ModManagerRegistry::getInstance();
    reg.executeOnModUnload([](std::string_view name) { clearMod(name); });
    reg.executeOnModEnable([](std::string_view name) { switchMod(name, true); });
    reg.executeOnModDisable([](std::string_view name) { switchMod(name, false); });
    return true;
}

ll::Expected<FunctionRef> exportFunc(
    std::string_view            nameSpace,
    std::string_view            funcName,
    CallbackFn&&                callback,
    bool                        returnExpected,
    std::weak_ptr<ll::mod::Mod> mod // NOLINT: performance-unnecessary-value-param
) {
    [[maybe_unused]] static bool registered = registerOnModUnload();
    if (nameSpace.find("::") != std::string::npos) {
        return error_utils::makeError(error_utils::ErrorReason::InvalidName, "Namespace can't includes \"::\"");
    }
    if (exportedFuncs.contains(std::pair{nameSpace, funcName})) {
        return error_utils::makeError(
            error_utils::ErrorReason::AlreadyExists,
            fmt::format("Fail to export! Function [{}::{}] already exists", nameSpace, funcName)
        );
    }
    if constexpr (IdentifierMapIsParallelMap)
        return exportedFuncs
            .emplace(
                std::make_pair(std::string(nameSpace), std::string(funcName)),
                ExportedFuncData(std::move(callback), std::move(mod), returnExpected)
            )
            .first->second;
    else
        return exportedFuncs
            .emplace(
                std::piecewise_construct,
                std::forward_as_tuple(nameSpace, funcName),
                std::forward_as_tuple(std::move(callback), std::move(mod), returnExpected)
            )
            .first->second;
}

ll::Expected<FunctionRef> importFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled) {
    auto iter = exportedFuncs.find(std::pair{nameSpace, funcName});
    if (iter == exportedFuncs.end()) [[unlikely]]
        return error_utils::makeNotFoundError(nameSpace, funcName);
    if (!includeDisabled && iter->second.disabled) [[unlikely]]
        return error_utils::makeDisabledError(nameSpace, funcName);
    return iter->second;
}


bool hasFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled) {
    auto iter = exportedFuncs.find(std::pair{nameSpace, funcName});
    if (iter == exportedFuncs.end()) return false;
    return includeDisabled || !iter->second.disabled;
}

std::weak_ptr<ll::mod::Mod> getProvider(std::string_view nameSpace, std::string_view funcName) {
    auto iter = exportedFuncs.find(std::pair{nameSpace, funcName});
    if (iter == exportedFuncs.end()) return {};
    return iter->second.provider;
}


bool removeFunc(std::string_view nameSpace, std::string_view funcName) {
    auto it = exportedFuncs.find(std::pair{nameSpace, funcName});
    if (it == exportedFuncs.end()) return false;
    exportedFuncs.erase(it);
    return true;
}

int removeNameSpace(std::string_view nameSpace) {
    int count = 0;
    for (auto iter = exportedFuncs.begin(); iter != exportedFuncs.end();) {
        if (iter->first.first == nameSpace) {
            iter = exportedFuncs.erase(iter);
            ++count;
        } else ++iter;
    }
    return count;
}

int removeFuncs(std::vector<std::pair<std::string, std::string>> const& funcs) {
    int count = 0;
    for (auto& [ns, name] : funcs) {
        if (removeFunc(ns, name)) count++;
    }
    return count;
}

void removeAllFunc() { exportedFuncs.clear(); }

} // namespace remote_call
