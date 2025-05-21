#include "RemoteCall.h"

#include "ll/api/Expected.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/Mod.h"
#include "ll/api/mod/ModManagerRegistry.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/GamingStatus.h"
#include "remote_call/core/LegacyRemoteCall.h"

namespace remote_call {

using CallableType = CallbackFn;

struct ExportedFuncData {
    CallableType                callable;
    std::weak_ptr<ll::mod::Mod> provider;
};

CallbackFn const                                               EMPTY_FUNC{};
std::unordered_map<std::string, remote_call::ExportedFuncData> exportedFuncs;

ll::io::Logger& getLogger() { return remote_call::LegacyRemoteCall::getInstance().getSelf().getLogger(); }

void clearMod(std::string_view name) {
    for (auto iter = exportedFuncs.begin(); iter != exportedFuncs.end();) {
        auto mod = iter->second.provider.lock();
        if (!mod || mod->getName() == name) {
            iter = exportedFuncs.erase(iter);
        } else ++iter;
    }
}

bool registerOnModUnload() {
    auto& reg = ll::mod::ModManagerRegistry::getInstance();
    reg.executeOnModUnload([](std::string_view name) { clearMod(name); });
    // reg.executeOnModDisable([](std::string_view name) {
    //     if (ll::getGamingStatus() == ll::GamingStatus::Running) {
    //         clearMod(name);
    //     }
    // });
    return true;
}

ll::Expected<> exportFunc(
    std::string const&          nameSpace,
    std::string const&          funcName,
    CallbackFn&&                callback,
    std::weak_ptr<ll::mod::Mod> mod
) {
    [[maybe_unused]] static bool registered = registerOnModUnload();
    if (nameSpace.find("::") != std::string::npos) {
        return ll::makeStringError("Namespace can't includes \"::\"");
    }
    if (exportedFuncs.count(nameSpace + "::" + funcName) != 0) {
        return ll::makeStringError(fmt::format("Fail to export! Function [{}::{}] already exists", nameSpace, funcName)
        );
    }
    exportedFuncs.emplace(nameSpace + "::" + funcName, ExportedFuncData{std::move(callback), std::move(mod)});
    return {};
}

ll::Expected<std::reference_wrapper<CallbackFn>> importFunc(std::string const& nameSpace, std::string const& funcName) {
    auto iter = exportedFuncs.find(nameSpace + "::" + funcName);
    if (iter == exportedFuncs.end())
        return ll::makeStringError(
            fmt::format("Fail to import! Function [{}::{}] has not been exported", nameSpace, funcName)
        );
    return iter->second.callable;
}

bool hasFunc(std::string const& nameSpace, std::string const& funcName) {
    return exportedFuncs.find(nameSpace + "::" + funcName) != exportedFuncs.end();
}

std::weak_ptr<ll::mod::Mod> getProvider(std::string const& nameSpace, std::string const& funcName) {
    auto iter = exportedFuncs.find(nameSpace + "::" + funcName);
    if (iter == exportedFuncs.end()) return {};
    return iter->second.provider;
}

bool removeFunc(std::string&& key) { return exportedFuncs.erase(key); }

bool removeFunc(std::string const& nameSpace, std::string const& funcName) {
    return removeFunc(nameSpace + "::" + funcName);
}

int removeNameSpace(std::string const& nameSpace) {
    int         count  = 0;
    std::string prefix = nameSpace + "::";
    for (auto iter = exportedFuncs.begin(); iter != exportedFuncs.end();) {
        if (nameSpace.starts_with(prefix)) {
            iter = exportedFuncs.erase(iter);
            ++count;
        } else ++iter;
    }
    return count;
}

int removeFuncs(std::vector<std::pair<std::string, std::string>> const& funcs) {
    int count = 0;
    for (auto& [ns, name] : funcs) {
        if (removeFunc(ns + "::" + name)) count++; // NOLINT: performance-inefficient-string-concatenation
    }
    return count;
}

void removeAllFunc() { exportedFuncs.clear(); }

} // namespace remote_call
