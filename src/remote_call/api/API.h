#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/FixedString.h"
#include "remote_call/api/impl/ExportImpl.h"
#include "remote_call/api/impl/ImportImpl.h"

// enable default conversions
#include "remote_call/api/conversions/DefaultContainerConversions.h" // IWYU pragma: keep
#include "remote_call/api/conversions/DefaultConversions.h"          // IWYU pragma: keep


namespace remote_call {

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
