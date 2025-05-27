// Modified from
// https://github.com/LiteLDev/LeviLamina/blob/18170e92c47f21a54378379c542a819533b7ab43/src/ll/api/reflection/Serialization.h
#pragma once

#include "ll/api/Expected.h"
#include "ll/api/reflection/Reflection.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/reflection/Reflection.h"
#include "remote_call/api/utils/ErrorUtils.h"

// Priority:
// 6. Custom serializeImpl
// 5. IsVectorBase IsDispatcher IsOptional
// 4. string
// 3. TupleLike
// 2. ArrayLike Associative
// 1. Reflectable enum
// 0. convertible

namespace remote_call::reflection {
using ll::Expected;
using ll::meta::PriorityTag;
using ll::reflection::Reflectable;

template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& vec, ll::meta::PriorityTag<5>)
    requires(ll::concepts::IsVectorBase<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& d, ll::meta::PriorityTag<5>)
    requires(ll::concepts::IsDispatcher<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& opt, ll::meta::PriorityTag<5>)
    requires(ll::concepts::IsOptional<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& str, ll::meta::PriorityTag<4>)
    requires(concepts::IsString<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& tuple, ll::meta::PriorityTag<3>)
    requires(concepts::IsTupleLike<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& arr, ll::meta::PriorityTag<2>)
    requires(ll::concepts::ArrayLike<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& map, ll::meta::PriorityTag<2>)
    requires(ll::concepts::Associative<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& obj, ll::meta::PriorityTag<1>)
    requires(Reflectable<std::remove_cvref_t<T>>);
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& e, ll::meta::PriorityTag<1>)
    requires(std::is_enum_v<std::remove_cvref_t<T>>);
// Disabled for DynamicValue to prevent unexpected conversion
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& obj, ll::meta::PriorityTag<0>)
    requires(std::convertible_to<std::remove_cvref_t<T>, J> && !concepts::IsDynamicValue<J>);

template <class J, class T>
[[nodiscard]] inline Expected<> serialize_to(J& j, T&& t) noexcept {
    return serializeImpl(j, std::forward<T>(t), priority::Hightest);
}

template <std::default_initializable J, class T>
[[nodiscard]] inline Expected<J> serialize(T&& t) noexcept
#if !defined(__INTELLISENSE__)
    requires(requires(J& j, T&& t) { serialize_to(j, std::forward<T>(t)); })
#endif
try {
    J              j{};
    ll::Expected<> res = serialize_to(j, std::forward<T>(t));
    if (res.has_value()) return j;
    return forwardError(res.error());
} catch (...) {
    return ll::makeExceptionError();
}


template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& vec, ll::meta::PriorityTag<5>)
    requires(ll::concepts::IsVectorBase<std::remove_cvref_t<T>>)
{
    j              = J::array();
    Expected<> res = {};
    std::remove_cvref_t<T>::forEachComponent([&]<typename axis_type, size_t iter> {
        if (res) {
            res = serialize_to(j.emplace_back(), std::forward<T>(vec).template get<axis_type, iter>());
            if (!res.has_value()) res = error_utils::makeSerIndexError(iter, res.error());
        }
    });
    return res;
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& d, ll::meta::PriorityTag<5>)
    requires(ll::concepts::IsDispatcher<std::remove_cvref_t<T>>)
{
    return serialize_to<J>(j, std::forward<T>(d).storage);
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& opt, ll::meta::PriorityTag<5>)
    requires(ll::concepts::IsOptional<std::remove_cvref_t<T>>)
{
    if (!opt) {
        j = nullptr;
        return {};
    }
    return serialize_to<J>(j, *std::forward<T>(opt));
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& str, ll::meta::PriorityTag<4>)
    requires(concepts::IsString<std::remove_cvref_t<T>>)
{
    j = std::string{std::forward<T>(str)};
    return {};
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& tuple, ll::meta::PriorityTag<3>)
    requires(concepts::IsTupleLike<std::remove_cvref_t<T>>)
{
    j                   = J::array();
    Expected<>     res  = {};
    constexpr auto impl = [](auto& ji, auto&& ti, auto& res, size_t i) {
        if (!res) return;
        res = serialize_to(ji, std::forward<decltype(ti)>(ti));
        if (!res) res = error_utils::makeSerIndexError(i, res.error());
    };
    (void)[&]<int... I>(std::index_sequence<I...>) {
        (void)(impl(j.emplace_back(), std::get<I>(std::forward<T>(tuple)), res, I), ...);
    }
    (std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{});
    return res;
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& arr, ll::meta::PriorityTag<2>)
    requires(ll::concepts::ArrayLike<std::remove_cvref_t<T>>)
{
    j              = J::array();
    Expected<> res = {};
    size_t     iter{0};
    for (auto&& val : std::forward<T>(arr)) {
        if (res) {
            res = serialize_to<J>(j.emplace_back(), std::forward<decltype(val)>(val));
            if (!res) {
                res = error_utils::makeSerIndexError(iter, res.error());
                break;
            }
            iter++;
        }
    }
    return res;
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& map, ll::meta::PriorityTag<2>)
    requires(ll::concepts::Associative<std::remove_cvref_t<T>>)
{
    using RT = std::remove_cvref_t<T>;
    static_assert(
        (concepts::IsString<typename RT::key_type> || std::is_enum_v<typename RT::key_type>),
        "the key type of the associative container must be convertible to a string"
    );
    j              = J::object();
    Expected<> res = {};
    for (auto&& [k, v] : map) {
        std::string key;
        if constexpr (std::is_enum_v<typename RT::key_type>) {
            key = magic_enum::enum_name(std::forward<decltype(k)>(k));
        } else {
            key = std::string{std::forward<decltype(k)>(k)};
        }
        res = serialize_to<J>(j[key], std::forward<decltype(v)>(v));
        if (!res) {
            res = error_utils::makeSerKeyError(key, res.error());
            break;
        }
    }
    if (j.size() != map.size()) __debugbreak();
    return res;
}

template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& obj, ll::meta::PriorityTag<1>)
    requires(Reflectable<std::remove_cvref_t<T>>)
{
    j              = J::object();
    Expected<> res = {};
    reflection::forEachMember(obj, [&](std::string_view name, auto&& member) {
        if (name.starts_with('$') || !res) {
            return;
        }
        using member_type = decltype(member);
        if constexpr (requires(member_type m) { serialize_to(j, std::forward<member_type>(m)); }) {
            res = serialize_to(j[name], std::forward<member_type>(member));
            if (!res) {
                res = error_utils::makeSerMemberError(std::string{name}, res.error());
            }
        } else {
            static_assert(ll::traits::always_false<member_type>, "this type can't serialize");
        }
    });
    return res;
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& e, ll::meta::PriorityTag<1>)
    requires(std::is_enum_v<std::remove_cvref_t<T>>)
{
    j = magic_enum::enum_name(std::forward<T>(e));
    return {};
}
template <class J, class T>
inline Expected<> serializeImpl(J& j, T&& obj, ll::meta::PriorityTag<0>)
    requires(std::convertible_to<std::remove_cvref_t<T>, J> && !concepts::IsDynamicValue<J>)
{
    j = J(std::forward<T>(obj));
    return {};
}
} // namespace remote_call::reflection
