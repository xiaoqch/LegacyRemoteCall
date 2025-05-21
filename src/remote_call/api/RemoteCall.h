#pragma once

#include "ll/api/Expected.h"
#include "ll/api/mod/NativeMod.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/TypeTraits.h"
#include "remote_call/api/reflection/Deserialization.h"
#include "remote_call/api/reflection/Serialization.h"
#include "remote_call/api/value/DynamicValue.h"

#include <remote_call/api/conversion/DefaultConversion.h>
#include <remote_call/api/reflection/SerializationExt.h>

namespace remote_call {

using CallbackFn = std::function<ll::Expected<DynamicValue>(std::vector<DynamicValue>&)>;

__declspec(dllexport) ll::Expected<> exportFunc(
    std::string const&          nameSpace,
    std::string const&          funcName,
    CallbackFn&&                callback,
    std::weak_ptr<ll::mod::Mod> mod = ll::mod::NativeMod::current()
);
__declspec(dllexport)      ll::Expected<std::reference_wrapper<CallbackFn>>
                           importFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllexport) bool hasFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllexport)      std::weak_ptr<ll::mod::Mod>
                           getProvider(std::string const& nameSpace, std::string const& funcName);
__declspec(dllexport) bool removeFunc(std::string const& nameSpace, std::string const& funcName);
__declspec(dllexport) int  removeNameSpace(std::string const& nameSpace);
__declspec(dllexport) int  removeFuncs(std::vector<std::pair<std::string, std::string>> const& funcs);


namespace impl {

using traits::function_traits;

template <typename T>
struct corrected_parameter {
    using hold_type                   = std::decay_t<T>;
    using arg_type                    = T;
    static constexpr bool add_pointer = false;
};

template <typename T>
    requires(std::is_reference_v<T>)
struct corrected_parameter<T> {
    using hold_type                   = std::decay_t<T>;
    using arg_type                    = T;
    static constexpr bool add_pointer = false;
};

// Player&, CompoundTag&...
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportFromValue<std::add_pointer_t<std::remove_cvref_t<T>>>)
struct corrected_parameter<T> {
    using hold_type                   = T;
    using arg_type                    = T;
    static constexpr bool add_pointer = true;
};


template <typename T>
    requires(corrected_parameter<T>::add_pointer)
T unpack(DynamicValue& v, ll::Expected<>& success) {
    std::decay_t<T>* p{};
    if (success) success = reflection::deserialize_to(v, p);
    return *p;
}
template <typename T>
    requires(!corrected_parameter<T>::add_pointer && requires(std::decay_t<T>&& val) { [](T) {}(std::move(val)); })
corrected_parameter<T>::hold_type unpack(DynamicValue& v, ll::Expected<>& success) {
    typename corrected_parameter<T>::hold_type p{};
    if (success) success = reflection::deserialize_to(v, p);
    return p;
}

/// TODO: Error message

template <typename Fn, typename Ret, typename... Args>
    requires(std::invocable<Fn, Args...>)
inline ll::Expected<> exportImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const& nameSpace,
    std::string const& funcName,
    Fn&&               callback
) {
    /// TODO: when Ret is ll::Excepted<>
    CallbackFn rawFunc = [callback = std::function(std::forward<decltype(callback)>(callback)
                          )](std::vector<DynamicValue>& args) -> ll::Expected<DynamicValue> {
        using LvarArgsTuple        = std::tuple<typename corrected_parameter<Args>::hold_type...>;
        constexpr size_t ArgsCount = sizeof...(Args);
        /// TODO: optional args
        if (ArgsCount != args.size()) {
            return ll::makeStringError(
                fmt::format("Argment count not match, requires {}, but got {}", ArgsCount, args.size())
            );
        }

        ll::Expected<> success{};
        LvarArgsTuple  paramTuple = [&]<int... I>(std::index_sequence<I...>) {
            return std::make_tuple(unpack<Args>(args[I], success)...);
        }(std::index_sequence_for<Args...>{});
        if (!success) return ll::forwardError(success.error());

        if constexpr (std::is_void_v<Ret>) {
            std::apply(callback, std::forward<decltype(paramTuple)>(paramTuple));
            return {};
        } else {
            auto      oriRet = std::apply(callback, std::forward<decltype(paramTuple)>(paramTuple));
            DynamicValue rcRet;
            success = reflection::serialize_to(rcRet, std::forward<decltype(oriRet)>(oriRet));
            if (!success) return ll::forwardError(success.error());
            return rcRet;
        }
    };
    return exportFunc(nameSpace, funcName, std::forward<CallbackFn>(rawFunc), ll::mod::NativeMod::current());
}


template <typename Ret, typename... Args>
inline auto importImpl(std::in_place_type_t<Ret(Args...)>, std::string const& nameSpace, std::string const& funcName) {
    /// TODO: when Ret is ll::Excepted<>
    using ExpectedRet = ll::Expected<std::remove_reference_t<Ret>>;
    return [nameSpace, funcName](Args... args) -> ExpectedRet {
        auto res = importFunc(nameSpace, funcName);
        if (!res) {
            return ll::makeStringError(
                fmt::format("Fail to import! Function [{}::{}] has not been exported", nameSpace, funcName)
            );
        }
        auto&     rawFunc    = res->get();
        DynamicValue params     = DynamicValue::array();
        auto      paramTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        ll::Expected<> success =
            reflection::serialize_to<decltype(params)>(params, std::forward<decltype(paramTuple)>(paramTuple));
        if (!success) return ll::forwardError(success.error());

        auto rcRet = rawFunc(params.get<ArrayType>());
        if (!rcRet) return ll::forwardError(rcRet.error());
        if constexpr (std::is_void_v<Ret>) return {};
        else {
            std::remove_reference_t<Ret> ret;
            success = reflection::deserialize_to<std::remove_reference_t<Ret>>(*rcRet, ret);
            if (!success) return ll::forwardError(success.error());
            return ret;
        }
    };
}

} // namespace impl

template <typename Fn>
    requires(requires(Fn&& fn) { std::function(std::forward<Fn>(fn)); })
inline auto importAs(std::string const& nameSpace, std::string const& funcName) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::importImpl(std::in_place_type<DecayedFn>, nameSpace, funcName);
}

template <typename Fn>
    requires(requires(Fn&& fn) { std::function(std::forward<Fn>(fn)); })
inline ll::Expected<> exportAs(std::string const& nameSpace, std::string const& funcName, Fn&& callback) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::exportImpl(std::in_place_type<DecayedFn>, nameSpace, funcName, std::forward<Fn>(callback));
}

} // namespace remote_call
