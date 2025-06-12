#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/FixedString.h"
#include "remote_call/api/impl/ExportImpl.h"
#include "remote_call/api/impl/ImportImpl.h"

// Enable default conversions
#include "remote_call/api/conversions/DefaultContainerConversions.h" // IWYU pragma: keep
#include "remote_call/api/conversions/DefaultConversions.h"          // IWYU pragma: keep

namespace remote_call {

/**
 * @brief Imports a remote function as a lambda proxy using runtime string identifiers.
 *
 * Always returns a lambda function that acts as a proxy for the exported remote function.
 * If the target function is not exported, calling this lambda will result in a runtime error.
 *
 * @tparam Fn Function type. Must be compatible with std::function.
 * @param nameSpace The namespace of the target function.
 * @param funcName The name of the target function.
 * @return Lambda function proxy for the exported function.
 * @note If the target function is not exported, invoking the returned lambda will produce a runtime error.
 * @see importAs() (consteval version) for compile-time string usage.
 */
template <typename Fn>
    requires(requires(Fn&& fn) { std::function(std::forward<Fn>(fn)); })
[[nodiscard]] constexpr auto importAs(std::string const& nameSpace, std::string const& funcName) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::importImpl(std::in_place_type<DecayedFn>, nameSpace, funcName);
}

/**
 * @brief Imports a remote function as a lambda proxy using compile-time string identifiers.
 *
 * Always returns a lambda function that acts as a proxy for the exported remote function.
 * If the target function is not exported, calling this lambda will result in a runtime error.
 *
 * @tparam Fn Function type. Must be compatible with std::function.
 * @tparam nameSpace Compile-time namespace of the target function (ll::FixedString).
 * @tparam funcName Compile-time name of the target function (ll::FixedString).
 * @return Lambda function proxy for the exported function.
 * @note If the target function is not exported, invoking the returned lambda will produce a runtime error.
 * @see importAs() (string version) for runtime string usage.
 */
template <typename Fn, ll::FixedString nameSpace, ll::FixedString funcName>
    requires(requires(Fn&& fn) { std::function(std::forward<Fn>(fn)); })
[[nodiscard]] consteval auto importAs() {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return []<typename Ret, typename... Args>(std::in_place_type_t<Ret(Args...)>) {
        return impl::importImpl<nameSpace, funcName, Ret, Args...>();
    }(std::in_place_type<DecayedFn>);
}

/**
 * @brief Imports a remote function with an explicit return type as a lambda proxy using runtime string identifiers.
 *
 * Always returns a lambda function that acts as a proxy for the exported remote function.
 * If the target function is not exported, calling this lambda will result in a runtime error.
 *
 * @tparam Ret Return type of the function.
 * @param nameSpace The namespace of the target function.
 * @param funcName The name of the target function.
 * @return Lambda function proxy for the exported function.
 * @note If the target function is not exported, invoking the returned lambda will produce a runtime error.
 * @see remote_call::importAs
 */
template <typename Ret>
[[nodiscard]] constexpr auto importEx(std::string const& nameSpace, std::string const& funcName) {
    return impl::importExImpl<Ret>(nameSpace, funcName);
}

/**
 * @brief Imports a remote function with an explicit return type as a lambda proxy using compile-time string
 * identifiers.
 *
 * Always returns a lambda function that acts as a proxy for the exported remote function.
 * If the target function is not exported, calling this lambda will result in a runtime error.
 *
 * @tparam Ret Return type of the function.
 * @tparam nameSpace Compile-time namespace of the target function (ll::FixedString).
 * @tparam funcName Compile-time name of the target function (ll::FixedString).
 * @return Lambda function proxy for the exported function.
 * @note If the target function is not exported, invoking the returned lambda will produce a runtime error.
 * @see remote_call::importAs
 */
template <typename Ret, ll::FixedString nameSpace, ll::FixedString funcName>
[[nodiscard]] consteval auto importEx() {
    return impl::importExImpl<nameSpace, funcName, Ret>();
}

/**
 * @brief Exports a local function for remote invocation, supporting default arguments.
 *
 * Registers the given function as a remote-callable function, optionally with default arguments.
 *
 * @tparam Fn Function type to be exported. Must be compatible with std::function.
 * @tparam DefaultArgs Types of default arguments.
 * @param nameSpace The namespace under which to export the function.
 * @param funcName The name for the exported function.
 * @param callback The callback function to export.
 * @param defaultArgs Optional default arguments for the function.
 * @return ll::Expected<> indicating success or failure.
 * @see exportEx for advanced export scenarios.
 */
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

/**
 * @brief Exports a local function for remote invocation with advanced wrapper support.
 *
 * Registers the given function wrapper as a remote-callable function.
 *
 * @tparam Fn Function type to be exported.
 * @tparam FuncWrapper Type of the function wrapper.
 * @param nameSpace The namespace under which to export the function.
 * @param funcName The name for the exported function.
 * @param callback The function wrapper to export.
 * @return ll::Expected<> indicating success or failure.
 * @see exportAs for simpler export scenarios.
 */
template <typename Fn, typename FuncWrapper>
inline ll::Expected<> exportEx(std::string const& nameSpace, std::string const& funcName, FuncWrapper&& callback) {
    using DecayedFn = traits::function_traits<decltype(std::function(std::declval<Fn>()))>::function_type;
    return impl::exportExImpl(std::in_place_type<DecayedFn>, nameSpace, funcName, std::forward<FuncWrapper>(callback))
        .transform([](auto&&) {});
}

/**
 * @def REMOTE_CALL_EXPORT_EX(nameSpace, funcName, fn)
 * @brief Macro for exporting a function as a remote-callable function with automatic wrapper generation.
 *
 * This macro simplifies the export of a function. It wraps the provided function in a lambda that automatically
 * forwards arguments.
 *
 * @param nameSpace The namespace under which to export the function.
 * @param funcName The name for the exported function.
 * @param fn The function to export.
 *
 * @see exportEx
 */
#define REMOTE_CALL_EXPORT_EX(nameSpace, funcName, fn)                                                                 \
    ::remote_call::exportEx<decltype(fn)>(                                                                             \
        nameSpace,                                                                                                     \
        funcName,                                                                                                      \
        [](auto&&... args) -> auto                                                                                     \
            requires requires() { fn(std::forward<decltype(args)>(args)...); }                                         \
        { return fn(std::forward<decltype(args)>(args)...); }                                                          \
        )

} // namespace remote_call
