#pragma once
#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "ll/api/mod/NativeMod.h"
#include "mc/nbt/CompoundTag.h"
#include "remote_call/api/ABI.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/TypeTraits.h"
#include "remote_call/api/conversions/AdlSerializer.h"
#include "remote_call/api/conversions/detail/Detail.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/Base.h"
#include "remote_call/api/value/DynamicValue.h"

#include <remote_call/api/conversions/DefaultContainerConversion.h>
#include <remote_call/api/conversions/DefaultConversion.h>


namespace remote_call {

namespace impl {

using traits::function_traits;

// remove const, volatile, reference, and add pointer if necessary
// real for get by DynamicValue
template <typename T>
struct corrected_arg {
    using hold_type                   = std::decay_t<T>;
    using real                        = std::decay_t<T>;
    static constexpr bool add_pointer = false;
};

// Player& -> Player*, CompoundTag& -> CompoundTag*...
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<std::add_pointer_t<std::remove_cvref_t<T>>>)
struct corrected_arg<T> {
    using hold_type                   = std::reference_wrapper<std::decay_t<T>>;
    using real                        = std::add_pointer_t<std::decay_t<T>>;
    static constexpr bool add_pointer = true;
};


template <typename T, size_t I = 0>
    requires(corrected_arg<T>::add_pointer)
typename corrected_arg<T>::hold_type unpack(DynamicValue& dv, ll::Expected<>& success) {
    typename corrected_arg<T>::real p{}; // Player*
    if (success) {
        success = dv.getTo(p);
        if (!success) success = error_utils::makeSerIndexError(I, success.error());
    }
    return std::ref(*p); // std::reference_wrapper<Player>
}
template <typename T, size_t I = 0>
    requires(!corrected_arg<T>::add_pointer)
corrected_arg<T>::hold_type unpack(DynamicValue& dv, ll::Expected<>& success) {
    typename corrected_arg<T>::real p{};
    if (success) {
        success = dv.getTo(p);
        if (!success) success = error_utils::makeSerIndexError(I, success.error());
    }
    return p;
}

template <typename Arg>
consteval void checkUptrType() {
    using DecayedArg = std::decay_t<Arg>;
    if constexpr (concepts::IsUniquePtr<DecayedArg>)
        static_assert(!std::is_lvalue_reference_v<Arg>, "Reference to unique_ptr is not allowed");
}
template <typename... Args>
consteval size_t getRequiredArgsCount() {
    size_t required = 0;
    size_t index    = 0;
    (
        [&]<typename T>() {
            index++;
            if constexpr (!ll::concepts::IsOptional<std::decay_t<T>> && !std::same_as<std::decay_t<T>, nullptr_t>) {
                required = index;
            }
        }.template operator()<Args>(),
        ...
    );
    return required;
}

template <typename Fn, typename Ret, typename... Args>
    requires(std::invocable<Fn, Args...>)
inline ll::Expected<> exportImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const& nameSpace,
    std::string const& funcName,
    Fn&&               callback
) {
    void((checkUptrType<Args>(), ...));
    /// TODO:
    CallbackFn rawFunc = [callback = std::forward<decltype(callback)>(callback),
                          nameSpace,
                          funcName](std::vector<DynamicValue>& args) mutable -> ll::Expected<DynamicValue> {
        using ArgsHoldingTuple     = std::tuple<typename corrected_arg<Args>::hold_type...>;
        constexpr size_t ArgsCount = sizeof...(Args);
        /// TODO: optional args
        constexpr size_t RequiredArgsCount = getRequiredArgsCount<Args...>();

        if constexpr (RequiredArgsCount > 0)
            if (args.size() < RequiredArgsCount) {
                return error_utils::makeArgsCountError<Ret, Args...>(nameSpace, funcName, args.size());
            }
        if constexpr (ArgsCount - RequiredArgsCount > 0)
            for (size_t i = args.size(); i < ArgsCount; ++i) args.emplace_back(NULL_VALUE);

        ll::Expected<>   success{};
        ArgsHoldingTuple paramsHolder = [&]<size_t... I>(std::index_sequence<I...>) {
            return std::make_tuple(unpack<Args, I>(args[I], success)...);
        }(std::index_sequence_for<Args...>{});
        if (!success)
            return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

        std::tuple<Args&&...> paramsRef = [&]<size_t... I>(std::index_sequence<I...>) {
            return std::forward_as_tuple(static_cast<Args&&>(std::get<I>(paramsHolder))...);
        }(std::index_sequence_for<Args...>{});

        if constexpr (std::is_void_v<Ret>) {
            std::apply(callback, std::forward<decltype(paramsRef)>(paramsRef));
            return {};
        } else if constexpr (concepts::IsLeviExpected<Ret>) {
            auto exceptedRet = std::apply(callback, std::forward<decltype(paramsRef)>(paramsRef));
            if (!exceptedRet) {
                return error_utils::makeCallError<Ret, Args...>(nameSpace, funcName, exceptedRet.error());
            }
            return DynamicValue::from(*exceptedRet)
                .or_else([&nameSpace, &funcName](auto&& err) -> ll::Expected<DynamicValue> {
                    return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "ret", err);
                });
        } else {
            Ret nativeRet = std::apply(callback, std::forward<decltype(paramsRef)>(paramsRef));
            return DynamicValue::from(std::forward<Ret>(nativeRet))
                .or_else([&nameSpace, &funcName](auto&& err) -> ll::Expected<DynamicValue> {
                    return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "ret", err);
                });
        }
    };
    return exportFunc(nameSpace, funcName, std::move(rawFunc), ll::mod::NativeMod::current());
}


template <typename Ret>
struct corrected_return {
    using Real     = Ret;
    using Excepted = decltype(std::declval<DynamicValue>().tryGet<Ret>());
};
template <typename Ret>
struct corrected_return<Ret const> : corrected_return<Ret> {};

template <typename Ret>
struct corrected_return<Ret&&> : corrected_return<Ret> {};

template <typename Ret>
    requires(!concepts::SupportFromDynamic<Ret*>)
struct corrected_return<Ret&> : corrected_return<Ret> {};

template <typename Ret>
struct corrected_return<ll::Expected<Ret>> : corrected_return<Ret> {};


template <typename Ret, typename... Args>
inline auto importImpl(std::in_place_type_t<Ret(Args...)>, std::string const& nameSpace, std::string const& funcName) {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    using RealRet     = corrected_return<Ret>::Real;
    return [nameSpace, funcName](Args... args) -> ExpectedRet {
        auto res = importFunc(nameSpace, funcName);
        if (!res) {
            return error_utils::makeNotFoundError(nameSpace, funcName);
        }
        auto&        rawFunc = res->get();
        DynamicValue params  = DynamicValue::array();
        params.get<ArrayType>().reserve(sizeof...(Args));
        auto paramTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        ll::Expected<> success = toDynamic(params, std::forward<decltype(paramTuple)>(paramTuple));
        if (!success)
            return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

        auto dynRet = rawFunc(params.get<ArrayType>());
        if (!dynRet) return error_utils::makeCallError<Ret, Args...>(nameSpace, funcName, dynRet.error());

        return dynRet->tryGet<RealRet>().or_else([&nameSpace, &funcName](auto&& err) -> ExpectedRet {
            return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "ret", err);
        });
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
