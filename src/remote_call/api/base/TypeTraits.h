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


template <typename T>
struct is_reference_wrapper : std::false_type {};
template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
constexpr bool is_reference_wrapper_v = is_reference_wrapper<T>::value;
template <typename T>
struct remove_reference_wrapper {
    using type = T;
};
template <typename T>
struct remove_reference_wrapper<std::reference_wrapper<T>> {
    using type = T&;
};
template <typename T>
using remove_reference_wrapper_t = typename remove_reference_wrapper<T>::type;
template <typename T>
using reference_to_pointer_t = std::add_pointer_t<std::remove_reference_t<remove_reference_wrapper_t<T>>>;
template <typename T>
using reference_to_wrapper_t = std::reference_wrapper<std::remove_reference_t<remove_reference_wrapper_t<T>>>;


} // namespace remote_call::traits