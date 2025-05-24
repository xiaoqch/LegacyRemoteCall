#pragma once

#include "ll/api/Expected.h"
#include "ll/api/mod/NativeMod.h"
#include "remote_call/api/ABI.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/TypeTraits.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/DynamicValue.h"

#include <remote_call/api/conversion/DefaultConversion.h>
#include <remote_call/api/conversion/DefaultStructureConversion.h>


namespace remote_call {

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
    requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<std::add_pointer_t<std::remove_cvref_t<T>>>)
struct corrected_parameter<T> {
    using hold_type                   = T;
    using arg_type                    = T;
    static constexpr bool add_pointer = true;
};


template <typename T, size_t I = 0>
    requires(corrected_parameter<T>::add_pointer)
T unpack(DynamicValue& dv, ll::Expected<>& success) {
    std::decay_t<T>* p{};
    if (success) {
        success = detail::fromDynamicInternal(dv, p);
        if (!success) success = error_utils::makeSerIndexError(I, success.error());
    }
    return *p;
}
template <typename T, size_t I = 0>
    requires(!corrected_parameter<T>::add_pointer && requires(std::decay_t<T>&& val) { [](T) {}(std::move(val)); })
corrected_parameter<T>::hold_type unpack(DynamicValue& dv, ll::Expected<>& success) {
    typename corrected_parameter<T>::hold_type p{};
    if (success) {
        success = detail::fromDynamicInternal(dv, p);
        if (!success) success = error_utils::makeSerIndexError(I, success.error());
    }
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
    CallbackFn rawFunc = [callback = std::function(std::forward<decltype(callback)>(callback)),
                          nameSpace,
                          funcName](std::vector<DynamicValue>& args) -> ll::Expected<DynamicValue> {
        using LvarArgsTuple        = std::tuple<typename corrected_parameter<Args>::hold_type...>;
        constexpr size_t ArgsCount = sizeof...(Args);
        /// TODO: optional args
        if (ArgsCount != args.size()) {
            return error_utils::makeArgsCountError<Ret, Args...>(nameSpace, funcName, args.size());
        }

        ll::Expected<> success{};
        LvarArgsTuple  paramTuple = [&]<int... I>(std::index_sequence<I...>) {
            return std::make_tuple(unpack<Args, I>(args[I], success)...);
        }(std::index_sequence_for<Args...>{});

        if (!success)
            return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

        if constexpr (std::is_void_v<Ret>) {
            std::apply(callback, std::forward<decltype(paramTuple)>(paramTuple));
            return {};
        } else {
            auto&&       oriRet = std::apply(callback, std::forward<decltype(paramTuple)>(paramTuple));
            DynamicValue rcRet;
            success = detail::toDynamicInternal(rcRet, std::forward<decltype(oriRet)>(oriRet));
            if (!success)
                return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "ret", success.error());
            return rcRet;
        }
    };
    return exportFunc(nameSpace, funcName, std::move(rawFunc), ll::mod::NativeMod::current());
}


template <typename Ret, typename... Args>
inline auto importImpl(std::in_place_type_t<Ret(Args...)>, std::string const& nameSpace, std::string const& funcName) {
    /// TODO: when Ret is ll::Excepted<>
    using ExpectedRet = ll::Expected<std::remove_reference_t<Ret>>;
    return [nameSpace, funcName](Args... args) -> ExpectedRet {
        auto res = importFunc(nameSpace, funcName);
        if (!res) {
            return error_utils::makeNotFoundError(nameSpace, funcName);
        }
        auto&        rawFunc    = res->get();
        DynamicValue params     = DynamicValue::array();
        auto         paramTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        ll::Expected<> success =
            detail::toDynamicInternal<decltype(paramTuple)>(params, std::forward<decltype(paramTuple)>(paramTuple));
        if (!success)
            return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

        auto rcRet = rawFunc(params.get<ArrayType>());
        if (!rcRet) return error_utils::makeCallError<Ret, Args...>(nameSpace, funcName, rcRet.error());

        if constexpr (std::is_void_v<Ret>) return {};
        else {
            auto&& ret = unpack<Ret>(*rcRet, success);
            // std::remove_reference_t<Ret> ret;
            // success = reflection::deserialize_to<std::remove_reference_t<Ret>>(*rcRet, ret);
            if (!success)
                return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "ret", success.error());
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
