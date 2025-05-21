#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/TypeTraits.h"

#include <functional>

namespace remote_call::traits {

template <typename T>
struct function_traits_impl;

template <typename Ret, typename... Args>
struct function_traits_impl<Ret(Args...)> : public ll::traits::function_traits<Ret(Args...)> {
    using return_type                  = Ret;
    using callback_fn                  = std::function<ll::Expected<Ret>(Args...)>;
    using legacy_callback_fn           = std::function<Ret(Args...)>;
    using args_tuple                   = std::tuple<Args...>;
    using lvar_args_tuple              = std::tuple<std::remove_cvref<Args>...>;
    static constexpr size_t args_count = sizeof...(Args);
};

template <typename T>
struct function_traits : public function_traits_impl<typename ll::traits::function_traits<T>::function_type> {};

template <typename VariantType, typename T>
struct is_variant_alternative;

template <typename... Types, typename T>
struct is_variant_alternative<std::variant<Types...>, T> : std::disjunction<std::is_same<T, Types>...> {};

template <typename VariantType, typename T>
inline constexpr bool is_variant_alternative_v = is_variant_alternative<VariantType, T>::value;

} // namespace remote_call::traits