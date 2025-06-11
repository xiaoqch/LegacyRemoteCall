#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "ll/api/base/FixedString.h"
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

// enable default conversions
#include "remote_call/api/conversions/DefaultContainerConversions.h" // IWYU pragma: keep
#include "remote_call/api/conversions/DefaultConversions.h"          // IWYU pragma: keep


namespace remote_call {

namespace impl {

#ifdef REMOTE_CALL_ALWAYS_RETURN_EXPECTED
constexpr bool ALWAYS_RETURN_EXPECTED = true;
#else
constexpr bool ALWAYS_RETURN_EXPECTED = false;
#endif

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
[[nodiscard]] inline corrected_arg<T>::hold_type unpack(DynamicValue& dv, ll::Expected<>& success) {
    typename corrected_arg<T>::real p{}; // Player*
    if (success) {
        success = dv.getTo(p);
        if (!success) success = error_utils::makeSerIndexError(I, success.error());
    }
    return std::ref(*p); // std::reference_wrapper<Player>
}
template <typename T, size_t I = 0>
    requires(!corrected_arg<T>::add_pointer)
[[nodiscard]] inline corrected_arg<T>::hold_type unpack(DynamicValue& dv, ll::Expected<>& success) {
    typename corrected_arg<T>::real p{};
    if (success) {
        success = dv.getTo(p);
        if (!success) success = error_utils::makeSerIndexError(I, success.error());
    }
    return p;
}

template <typename T, size_t I = 0, size_t Req = 0, typename Def = std::tuple<>>
    requires(corrected_arg<T>::add_pointer)
[[nodiscard]] inline corrected_arg<T>::hold_type
unpackWithDefault(std::vector<DynamicValue>& dv, ll::Expected<>& success, Def const& defaultArgs) {
    if constexpr (I >= Req) {
        if (success && (I >= dv.size() || dv[I].is_null())) {
            return std::get<I - Req>(defaultArgs);
        }
    }
    return unpack<T>(dv[I], success);
}
template <typename T, size_t I = 0, size_t Req = I, typename Def = std::tuple<>>
    requires(!corrected_arg<T>::add_pointer)
[[nodiscard]] inline corrected_arg<T>::hold_type
unpackWithDefault(std::vector<DynamicValue>& dv, ll::Expected<>& success, Def const& defaultArgs) {
    if constexpr (I >= Req) {
        if (success && (I >= dv.size() || dv[I].is_null())) {
            return std::get<I - Req>(defaultArgs);
        }
    }
    return unpack<T>(dv[I], success);
}

template <typename Arg>
consteval void checkUptrType() {
    using DecayedArg = std::decay_t<Arg>;
    if constexpr (concepts::IsUniquePtr<DecayedArg>)
        static_assert(!std::is_lvalue_reference_v<Arg>, "Reference to unique_ptr is not allowed");
}

template <size_t Max, typename... Args>
consteval size_t getNonOptionalArgsCount() {
    size_t required = 0;
    size_t index    = 0;
    (
        [&]<typename T>() {
            index++;
            if (index > Max) return;
            if constexpr (!ll::concepts::IsOptional<std::decay_t<T>> && !std::same_as<std::decay_t<T>, nullptr_t>) {
                required = index;
            }
        }.template operator()<Args>(),
        ...
    );
    return required;
}

template <typename Fn, std::size_t N, typename... Args>
consteval bool isInvocableWithNArgs() {
    return []<size_t... I>(std::index_sequence<I...>) -> bool {
        return std::is_invocable_v<Fn, std::tuple_element_t<I, std::tuple<Args...>>...>;
    }(std::make_index_sequence<N>());
}

template <typename... Args, typename Fn>
consteval size_t getRequiredArgsCount(std::in_place_type_t<Fn>)
    requires(std::invocable<Fn, Args...>)
{
    size_t required = 0;
    [&]<size_t... N>(std::index_sequence<N...>) {
        (
            [&]<size_t I>() {
                if constexpr (!isInvocableWithNArgs<Fn, I, Args...>()) {
                    required = I + 1;
                }
            }.template operator()<N>(),
            ...
        );
    }(std::index_sequence_for<Args...>{});
    return required;
}


template <typename Fn, typename Ret, typename... Args, typename... DefaultArgs>
[[nodiscard]] inline ll::Expected<DynamicValue> exportCallImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const&                                 nameSpace,
    std::string const&                                 funcName,
    Fn&&                                               callback,
    std::vector<DynamicValue>&                         args,
    [[maybe_unused]] std::tuple<DefaultArgs...> const& defaultArgsTuple = {}
) {
    using ArgsHoldingTuple = std::tuple<typename corrected_arg<Args>::hold_type...>;
    ll::Expected<>   success{};
    ArgsHoldingTuple paramsHolder = [&]<size_t... I>(std::index_sequence<I...>) {
        if constexpr (sizeof...(DefaultArgs) > 0) {
            return std::make_tuple(
                unpackWithDefault<Args, I, sizeof...(Args) - sizeof...(DefaultArgs)>(args, success, defaultArgsTuple)...
            );
        } else {
            return std::make_tuple(unpack<Args, I>(args[I], success)...);
        }
    }(std::index_sequence_for<Args...>{});
    if (!success) return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

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
}

template <size_t ArgsCount, size_t RequiredCount, typename Fn, typename Ret, typename... Args>
[[nodiscard]] inline ll::Expected<DynamicValue> exportExCallImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const&         nameSpace,
    std::string const&         funcName,
    Fn&&                       callback,
    std::vector<DynamicValue>& args,
    size_t                     argsCount
) {
    if constexpr (ArgsCount > RequiredCount) {
        if (argsCount < ArgsCount) {
            return exportExCallImpl<ArgsCount - 1, RequiredCount>(
                std::in_place_type<Ret(Args...)>,
                nameSpace,
                funcName,
                std::forward<Fn>(callback),
                args,
                argsCount
            );
        }
    }
    constexpr auto FunctionTag = [&]<size_t... I>(std::index_sequence<I...>) -> auto {
        using RawTuple = std::tuple<Args...>;
        return std::in_place_type<Ret(std::tuple_element_t<I, RawTuple>...)>;
    }(std::make_index_sequence<ArgsCount>());
    return exportCallImpl(FunctionTag, nameSpace, funcName, std::forward<Fn>(callback), args);
}

template <size_t ArgsCount, size_t RequiredCount, size_t NonOptionalArgsCount>
[[nodiscard]] inline bool normalizeArgs(std::vector<DynamicValue>& args) {
    if constexpr (NonOptionalArgsCount > 0)
        if (args.size() < NonOptionalArgsCount) {
            return false;
        }

    if constexpr (RequiredCount > 0) {
        size_t argsCount = std::min(args.size(), ArgsCount);
        size_t dropCount = 0;
        if (argsCount > RequiredCount) {
            // Drop Null
            // if (args.size() > sizeof...(Args)) dropCount = args.size() - sizeof...(Args);
            for (auto& arg : std::ranges::reverse_view(args) | std::views::drop(dropCount)) {
                if (argsCount > RequiredCount && arg.hold<NullType>()) {
                    argsCount--;
                } else {
                    break;
                }
            }
        } else if (argsCount < RequiredCount) {
            // Fill Null
            argsCount = RequiredCount;
        }
        args.resize(argsCount);
    }
    return true;
}

template <typename Fn, typename Ret, typename... Args>
    requires(std::invocable<Fn, Args...>)
[[nodiscard]] inline ll::Expected<FunctionRef> exportExImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const& nameSpace,
    std::string const& funcName,
    Fn&&               callback
) {
    void((checkUptrType<Args>(), ...));
    CallbackFn rawFunc = [callback = std::forward<decltype(callback)>(callback),
                          nameSpace,
                          funcName](std::vector<DynamicValue> args) mutable -> ll::Expected<DynamicValue> {
        // args: int, optional<int>, int = 2
        // RequiredArgsCount = 2, NonOptionalArgsCount = 1
        constexpr size_t                  ArgsCount            = sizeof...(Args);
        constexpr size_t                  RequiredArgsCount    = getRequiredArgsCount<Args...>(std::in_place_type<Fn>);
        constexpr size_t                  NonOptionalArgsCount = getNonOptionalArgsCount<RequiredArgsCount, Args...>();
        [[maybe_unused]] constexpr size_t DefaultArgsCount     = ArgsCount - RequiredArgsCount;

        if (!normalizeArgs<ArgsCount, RequiredArgsCount, NonOptionalArgsCount>(args)) {
            return error_utils::makeArgsCountError<Ret, Args...>(nameSpace, funcName, args.size());
        }
        return exportExCallImpl<ArgsCount, RequiredArgsCount>(
            std::in_place_type<Ret(Args...)>,
            nameSpace,
            funcName,
            callback,
            args,
            args.size()
        );
    };
    constexpr auto returnExpected = ALWAYS_RETURN_EXPECTED || ll::concepts::IsLeviExpected<Ret>;
    return exportFunc(nameSpace, funcName, std::move(rawFunc), returnExpected, ll::mod::NativeMod::current());
}

template <typename Fn, typename Ret, typename... Args, typename... DefaultArgs>
    requires(std::invocable<Fn, Args...>)
[[nodiscard]] inline ll::Expected<FunctionRef> exportImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const& nameSpace,
    std::string const& funcName,
    Fn&&               callback,
    DefaultArgs&&... defaultArgs
) {
    void((checkUptrType<Args>(), ...));
    CallbackFn rawFunc = [callback         = std::forward<decltype(callback)>(callback),
                          defaultArgsTuple = std::make_tuple(std::forward<DefaultArgs>(defaultArgs)...),
                          nameSpace,
                          funcName](std::vector<DynamicValue> args) mutable -> ll::Expected<DynamicValue> {
        constexpr size_t ArgsCount            = sizeof...(Args);
        constexpr size_t DefaultArgsCount     = sizeof...(DefaultArgs);
        constexpr size_t RequiredArgsCount    = ArgsCount - DefaultArgsCount;
        constexpr size_t NonOptionalArgsCount = getNonOptionalArgsCount<RequiredArgsCount, Args...>();

        if (!normalizeArgs<ArgsCount, RequiredArgsCount, NonOptionalArgsCount>(args)) {
            return error_utils::makeArgsCountError<Ret, Args...>(nameSpace, funcName, args.size());
        }
        return exportCallImpl(std::in_place_type<Ret(Args...)>, nameSpace, funcName, callback, args, defaultArgsTuple);
    };
    constexpr auto returnExpected = ALWAYS_RETURN_EXPECTED || ll::concepts::IsLeviExpected<Ret>;
    return exportFunc(nameSpace, funcName, std::move(rawFunc), returnExpected, ll::mod::NativeMod::current());
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
[[nodiscard]] inline corrected_return<Ret>::Excepted
importCallImpl(std::string const& nameSpace, std::string const& funcName, Args&&... args) {
    using ExpectedRet = corrected_return<Ret>::Excepted;
    using RealRet     = corrected_return<Ret>::Real;

    auto res = importFunc(nameSpace, funcName);
    if (!res) {
        return ll::forwardError(res.error());
    }
    auto const&  rawFunc = *res;
    DynamicValue params  = DynamicValue::array();
    params.get<ArrayType>().reserve(sizeof...(Args));
    auto paramTuple = std::forward_as_tuple(std::forward<Args>(args)...);

    ll::Expected<> success = toDynamic(params, std::forward<decltype(paramTuple)>(paramTuple));
    if (!success) return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

    auto dynRet = rawFunc(std::move(params).get<ArrayType>());
    if (!dynRet) return error_utils::makeCallError<Ret, Args...>(nameSpace, funcName, dynRet.error());

    return dynRet->tryGet<RealRet>().or_else([&nameSpace, &funcName](auto&& err) -> ExpectedRet {
        return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "ret", err);
    });
}


template <typename Ret, typename... Args>
[[nodiscard]] inline auto
importImpl(std::in_place_type_t<Ret(Args...)>, std::string const& nameSpace, std::string const& funcName) {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [nameSpace, funcName](Args... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace, funcName, std::forward<decltype(args)>(args)...);
    };
}

template <ll::FixedString nameSpace, ll::FixedString funcName, typename Ret, typename... Args>
[[nodiscard]] consteval auto importImpl() {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [](Args... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace.str(), funcName.str(), std::forward<decltype(args)>(args)...);
    };
}

template <typename Ret>
[[nodiscard]] inline auto importExImpl(std::string const& nameSpace, std::string const& funcName) {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [nameSpace, funcName](auto&&... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace, funcName, std::forward<decltype(args)>(args)...);
    };
}

template <ll::FixedString nameSpace, ll::FixedString funcName, typename Ret>
[[nodiscard]] consteval auto importExImpl() {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [](auto&&... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace.str(), funcName.str(), std::forward<decltype(args)>(args)...);
    };
}

} // namespace impl

template <typename Fn>
    requires(requires(Fn&& fn) { std::function(std::forward<Fn>(fn)); })
[[nodiscard]] inline auto importAs(std::string const& nameSpace, std::string const& funcName) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::importImpl(std::in_place_type<DecayedFn>, nameSpace, funcName);
}

template <typename Fn, ll::FixedString nameSpace, ll::FixedString funcName>
    requires(requires(Fn&& fn) { std::function(std::forward<Fn>(fn)); })
[[nodiscard]] consteval auto importAs() {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return []<typename Ret, typename... Args>(std::in_place_type_t<Ret(Args...)>) {
        return impl::importImpl<nameSpace, funcName, Ret, Args...>();
    }(std::in_place_type<DecayedFn>);
}

template <typename Ret>
[[nodiscard]] inline auto importEx(std::string const& nameSpace, std::string const& funcName) {
    return impl::importExImpl<Ret>(nameSpace, funcName);
}

template <typename Ret, ll::FixedString nameSpace, ll::FixedString funcName>
[[nodiscard]] consteval auto importEx() {
    return impl::importExImpl<nameSpace, funcName, Ret>();
}

template <typename Fn, typename... DefaultArgs>
inline ll::Expected<>
exportAs(std::string const& nameSpace, std::string const& funcName, Fn&& callback, DefaultArgs&&... defaultArgs) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::exportImpl(
               std::in_place_type<DecayedFn>,
               nameSpace,
               funcName,
               std::forward<Fn>(callback),
               std::forward<DefaultArgs>(defaultArgs)...
    )
        .transform([](auto&&) {});
}

template <typename Fn, typename FuncWrapper>
inline ll::Expected<> exportEx(std::string const& nameSpace, std::string const& funcName, FuncWrapper&& callback) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::exportExImpl(std::in_place_type<DecayedFn>, nameSpace, funcName, std::forward<FuncWrapper>(callback))
        .transform([](auto&&) {});
}

#define REMOTE_CALL_EXPORT_EX(nameSpace, funcName, fn)                                                                 \
    ::remote_call::exportEx<decltype(fn)>(                                                                             \
        nameSpace,                                                                                                     \
        funcName,                                                                                                      \
        [](auto&&... args) -> auto                                                                                     \
            requires requires() { fn(std::forward<decltype(args)>(args)...); }                                         \
        { return fn(std::forward<decltype(args)>(args)...); }                                                          \
        )

} // namespace remote_call
