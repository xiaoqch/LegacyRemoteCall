#include "Compatibility.h"

#include "ll/api/Expected.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"
#include "remote_call/api/RemoteCall.h"
#include "remote_call/api/value/DynamicValue.h"
#include "remote_call/core/LegacyRemoteCall.h"


namespace RemoteCall {

CallbackFn const EMPTY_FUNC{};

inline ll::io::Logger& getLogger() { return remote_call::LegacyRemoteCall::getInstance().getSelf().getLogger(); }

bool exportFunc(std::string const& nameSpace, std::string const& funcName, CallbackFn&& callback, void* handle) {
    auto           mod    = ll::mod::NativeMod::getByHandle(handle);
    ll::Expected<> result = remote_call::exportFunc(
        nameSpace,
        funcName,
        [callback =
             std::move(callback)](std::vector<remote_call::DynamicValue>& args) -> ll::Expected<remote_call::DynamicValue> {
            return callback(std::move(reinterpret_cast<std::vector<ValueType>&>(args)));
        },
        mod
    );
    if (!result) {
        mod->getLogger().error(result.error().message());
    }
    return result.has_value();
}
std::unordered_map<std::string, CallbackFn> cached;
CallbackFn const&                           importFunc(std::string const& nameSpace, std::string const& funcName) {
    if (!remote_call::hasFunc(nameSpace, funcName)) return EMPTY_FUNC;
    return cached
        .emplace(
            nameSpace + "::" + funcName,
            [nameSpace, funcName](std::vector<ValueType> args) -> ValueType {
                auto func = remote_call::importFunc(nameSpace, funcName);
                if (!func) {
                    _onCallError(
                        fmt::format("Fail to import! Function [{}::{}] has not been exported", nameSpace, funcName)
                    );
                    return {nullptr};
                }
                auto res = func->get()(reinterpret_cast<std::vector<remote_call::DynamicValue>&>(args));
                if (res) return std::move(reinterpret_cast<ValueType&>(*res));
                _onCallError(fmt::format(
                    "Fail to invoke Function [{}::{}], reason: {}",
                    nameSpace,
                    funcName,
                    res.error().message()
                ));
                return {nullptr};
            }
        )
        .first->second;
}

bool hasFunc(std::string const& nameSpace, std::string const& funcName) {
    return remote_call::hasFunc(nameSpace, funcName);
}

bool removeFunc(std::string const& nameSpace, std::string const& funcName) {
    return remote_call::removeFunc(nameSpace, funcName);
}

void _onCallError(std::string const& msg, void* handle) {
    getLogger().error(msg);
    auto plugin = ll::mod::NativeMod::getByHandle(handle);
    if (plugin) getLogger().error("In plugin <{}>", plugin->getManifest().name);
}

int removeNameSpace(std::string const& nameSpace) { return remote_call::removeNameSpace(nameSpace); }

int removeFuncs(std::vector<std::pair<std::string, std::string>>& funcs) { return remote_call::removeFuncs(funcs); }

} // namespace RemoteCall

static_assert(
    std::derived_from<
        remote_call::DynamicValue,
        std::variant<
            std::variant<
                bool,
                std::string,
                std::nullptr_t,
                remote_call::NumberType,
                Player*,
                Actor*,
                BlockActor*,
                Container*,
                remote_call::WorldPosType,
                remote_call::BlockPosType,
                remote_call::ItemType,
                remote_call::BlockType,
                remote_call::NbtType>,
            std::vector<remote_call::DynamicValue>,
            std::unordered_map<std::string, remote_call::DynamicValue>>>,
    "the 'src/remote_call/api/legacy/*' files should be removed if DynamicValue changed"
);
