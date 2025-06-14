#pragma once

#include "Common.h"
#include "remote_call/api/ABI.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call::impl {

#ifdef REMOTE_CALL_ALWAYS_RETURN_EXPECTED
inline constexpr bool ALWAYS_RETURN_EXPECTED = true;
#else
inline constexpr bool ALWAYS_RETURN_EXPECTED = false;
#endif

// remove const, volatile, reference, and add pointer if necessary
// real for get by DynamicValue
template <typename T>
struct corrected_arg {
    using hold_type                           = std::decay_t<T>;
    using real                                = std::decay_t<T>;
    static inline constexpr bool ref_from_ptr = false;
};

// Player& -> Player*, CompoundTag& -> CompoundTag*...
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<std::add_pointer_t<std::remove_cvref_t<T>>>)
struct corrected_arg<T> {
    using hold_type                           = traits::reference_to_wrapper_t<T>;
    using real                                = std::add_pointer_t<std::decay_t<T>>;
    static inline constexpr bool ref_from_ptr = true;
};

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


template <typename Tpl, size_t... I, typename... DefaultArgs>
ll::Expected<Tpl> fromDynamicTupleWithDefaultImpl(
    DynamicArray& da,
    std::index_sequence<I...>,
    std::tuple<DefaultArgs...> const& defArgs = {}
) {
    ll::Expected<> res;
    auto           getElement = [&]<size_t Idx, typename Element>(DynamicArray& da, ll::Expected<>& res)
        -> decltype(da[Idx].tryGet<Element>(res)) {
        if constexpr (!requires() { da[Idx].tryGet<Element>(res); }) {
            __debugbreak();
            static_assert(ll::traits::always_false<Element>, "this type can't deserialize");
        }
        if (!res) [[unlikely]]
            return std::nullopt;
        if (Idx < da.size() && !da[Idx].hold<NullType>()) {
            auto opt = da[Idx].tryGet<Element>(res);
            if (!res) [[unlikely]]
                res = error_utils::makeSerIndexError(Idx, res.error());
            return opt;
        } else {
            constexpr size_t Req = sizeof...(I) - sizeof...(DefaultArgs);
            if constexpr (Idx >= Req) {
                return std::get<Idx - Req>(defArgs);
            } else if constexpr (ll::concepts::IsOptional<Element>) {
                return std::make_optional<Element>(std::nullopt);
            } else {
                res = error_utils::makeError(
                    error_utils::ErrorReason::IndexOutOfRange,
                    fmt::format("index \"{}\" outof range", Idx)
                );
                res = error_utils::makeSerIndexError(Idx, res.error());
                return std::nullopt;
            }
        }
    };
    auto tmp = std::make_tuple(getElement.template operator()<I, std::tuple_element_t<I, Tpl>>(da, res)...);
    if (res) [[likely]]
        return Tpl{*std::get<I>(std::move(tmp))...};
    return ll::forwardError(res.error());
}

template <typename Fn, typename Ret, typename... Args, typename... DefaultArgs>
[[nodiscard]] inline ll::Expected<DynamicValue> exportCallImpl(
    std::in_place_type_t<Ret(Args...)>,
    std::string const&                                 nameSpace,
    std::string const&                                 funcName,
    Fn&&                                               callback,
    std::vector<DynamicValue>&&                        args,
    [[maybe_unused]] std::tuple<DefaultArgs...> const& defArgs = {}
) {
    using ArgsHoldingTuple = std::tuple<typename corrected_arg<Args>::hold_type...>;
    ll::Expected<ArgsHoldingTuple> paramsHolder =
        fromDynamicTupleWithDefaultImpl<ArgsHoldingTuple>(args, std::index_sequence_for<Args...>{}, defArgs);

    if (!paramsHolder) [[unlikely]]
        return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "args", paramsHolder.error());

    // For support T& param
    std::tuple<Args&&...> paramsRef = []<size_t... I>(std::index_sequence<I...>, auto&& params) {
        return std::forward_as_tuple(static_cast<Args&&>(std::get<I>(params))...);
    }(std::index_sequence_for<Args...>{}, *paramsHolder);

    if constexpr (std::is_void_v<Ret>) {
        std::apply(callback, std::forward<decltype(paramsRef)>(paramsRef));
        return {};
    } else if constexpr (concepts::IsLeviExpected<Ret>) {
        auto exceptedRet = std::apply(callback, std::forward<decltype(paramsRef)>(paramsRef));
        if (!exceptedRet) [[unlikely]] {
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
    std::string const&          nameSpace,
    std::string const&          funcName,
    Fn&&                        callback,
    std::vector<DynamicValue>&& args,
    size_t                      argsCount
) {
    if constexpr (ArgsCount > RequiredCount) {
        if (argsCount < ArgsCount) {
            return exportExCallImpl<ArgsCount - 1, RequiredCount>(
                std::in_place_type<Ret(Args...)>,
                nameSpace,
                funcName,
                std::forward<Fn>(callback),
                std::move(args),
                argsCount
            );
        }
    }
    constexpr auto FunctionTag = [&]<size_t... I>(std::index_sequence<I...>) -> auto {
        using RawTuple = std::tuple<Args...>;
        return std::in_place_type<Ret(std::tuple_element_t<I, RawTuple>...)>;
    }(std::make_index_sequence<ArgsCount>());
    return exportCallImpl(FunctionTag, nameSpace, funcName, std::forward<Fn>(callback), std::move(args));
}

template <size_t ArgsCount, size_t RequiredCount, size_t NonOptionalArgsCount>
[[nodiscard]] LL_FORCEINLINE constexpr bool normalizeArgs(std::vector<DynamicValue>& args) {
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
    CallbackFn rawFunc = [callback = std::forward<decltype(callback)>(callback),
                          defArgs  = std::make_tuple(std::forward<DefaultArgs>(defaultArgs)...),
                          nameSpace,
                          funcName](std::vector<DynamicValue>&& args) mutable -> ll::Expected<DynamicValue> {
        // args: int, optional<int>, int = 2
        // RequiredArgsCount = 2, NonOptionalArgsCount = 1
        constexpr size_t ArgsCount            = sizeof...(Args);
        constexpr size_t DefaultArgsCount     = sizeof...(DefaultArgs);
        constexpr size_t RequiredArgsCount    = ArgsCount - DefaultArgsCount;
        constexpr size_t NonOptionalArgsCount = getNonOptionalArgsCount<RequiredArgsCount, Args...>();

        if (!normalizeArgs<ArgsCount, RequiredArgsCount, NonOptionalArgsCount>(args)) [[unlikely]] {
            return error_utils::makeArgsCountError<Ret, Args...>(nameSpace, funcName, args.size());
        }
        return exportCallImpl(
            std::in_place_type<Ret(Args...)>,
            nameSpace,
            funcName,
            callback,
            std::move(args),
            defArgs
        );
    };
    constexpr auto returnExpected = ALWAYS_RETURN_EXPECTED || ll::concepts::IsLeviExpected<Ret>;
    return exportFunc(nameSpace, funcName, std::move(rawFunc), returnExpected, ll::mod::NativeMod::current());
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
                          funcName](std::vector<DynamicValue>&& args) mutable -> ll::Expected<DynamicValue> {
        constexpr size_t                  ArgsCount            = sizeof...(Args);
        constexpr size_t                  RequiredArgsCount    = getRequiredArgsCount<Args...>(std::in_place_type<Fn>);
        constexpr size_t                  NonOptionalArgsCount = getNonOptionalArgsCount<RequiredArgsCount, Args...>();
        [[maybe_unused]] constexpr size_t DefaultArgsCount     = ArgsCount - RequiredArgsCount;

        if (!normalizeArgs<ArgsCount, RequiredArgsCount, NonOptionalArgsCount>(args)) [[unlikely]] {
            return error_utils::makeArgsCountError<Ret, Args...>(nameSpace, funcName, args.size());
        }
        return exportExCallImpl<ArgsCount, RequiredArgsCount>(
            std::in_place_type<Ret(Args...)>,
            nameSpace,
            funcName,
            callback,
            std::move(args),
            args.size()
        );
    };
    constexpr auto returnExpected = ALWAYS_RETURN_EXPECTED || ll::concepts::IsLeviExpected<Ret>;
    return exportFunc(nameSpace, funcName, std::move(rawFunc), returnExpected, ll::mod::NativeMod::current());
}

} // namespace remote_call::impl
