#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "ll/api/reflection/Reflection.h"
#include "magic_enum.hpp"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/reflection/Reflection.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/DynamicValue.h"

#include <optional>



namespace remote_call {


// std::optional<T>
template <typename T>
    requires(ll::concepts::IsOptional<std::remove_cvref_t<T>> && concepts::SupportToDynamic<typename std::remove_cvref_t<T>::value_type>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& t, priority::HightTag) {
    if (t.has_value()) return detail::toDynamicInternal(dv, *std::forward<T>(t));
    else dv.emplace<ElementType>(NullValue);
    return {};
}

template <ll::concepts::IsOptional T>
    requires(concepts::SupportFromDynamic<typename T::value_type>)
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& t, priority::HightTag) {
    if (dv.hold<NullType>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = detail::fromDynamicInternal(dv, val);
        if (res) t = val;
        return res;
    }
}

// vector base
template <class T>
    requires(ll::concepts::IsVectorBase<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& vec, ll::meta::PriorityTag<2>) {
    dv                 = DynamicValue::array();
    ll::Expected<> res = {};
    std::remove_cvref_t<T>::forEachComponent([&]<typename axis_type, size_t iter> {
        if (res) {
            res = detail::toDynamicInternal(dv.emplace_back(), std::forward<T>(vec).template get<axis_type, iter>());
            if (!res.has_value()) res = error_utils::makeSerIndexError(iter, res.error());
        }
    });
    return res;
}
template <ll::concepts::IsVectorBase T>
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& vec, ll::meta::PriorityTag<2>) {
    ll::Expected<> res{};
    T::forEachComponent([&]<typename axis_type, size_t iter> {
        if (res) {
            res = detail::fromDynamicInternal<axis_type>(dv[iter], vec.template get<axis_type, iter>());
            if (!res) res = error_utils::makeSerIndexError(iter, res.error());
        }
    });
    return res;
}

// tuple
template <class T>
    requires(concepts::IsTupleLike<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& tuple, ll::meta::PriorityTag<2>) {
    ll::Expected<> res  = {};
    dv                  = DynamicValue::array();
    constexpr auto impl = [](auto& ji, auto&& ti, auto& res, size_t i) {
        if (!res) return;
        res = detail::toDynamicInternal(ji, std::forward<decltype(ti)>(ti));
        if (!res) res = error_utils::makeSerIndexError(i, res.error());
    };
    (void)[&]<int... I>(std::index_sequence<I...>) {
        (void)(impl(dv.emplace_back(), std::get<I>(std::forward<T>(tuple)), res, I), ...);
    }
    (std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{});
    return res;
}
template <concepts::IsTupleLike T>
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& tuple, ll::meta::PriorityTag<2>) {
    if (!dv.is_array()) return error_utils::makeFromDynamicTypeError<T, ArrayType>(dv);
    if (dv.size() != std::tuple_size_v<T>)
        return error_utils::makeError(
            error_utils::ErrorReason::IndexOutOfRange,
            fmt::format("array size must be {}", std::tuple_size_v<T>)
        );
    ll::Expected<> res{};

    constexpr auto func = [](auto& dv, auto& ti, auto& res, size_t i) {
        if (!res) return;
        res = detail::fromDynamicInternal(dv, ti);
        if (!res) res = error_utils::makeSerIndexError(i, res.error());
    };
    (void)[&]<int... I>(std::index_sequence<I...>) { (void)(func(dv[I], std::get<I>(tuple), res, I), ...); }
    (std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{});
    return res;
}

// array
template <class T>
    requires(ll::concepts::ArrayLike<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& arr, ll::meta::PriorityTag<1>) {
    ll::Expected<> res = {};
    dv                 = DynamicValue::array();
    size_t iter{0};
    for (auto&& val : std::forward<T>(arr)) {
        if (res) {
            res = detail::toDynamicInternal(dv.emplace_back(), std::forward<decltype((val))>(val));
            if (!res) {
                res = error_utils::makeSerIndexError(iter, res.error());
                break;
            }
            iter++;
        }
    }
    return res;
}
template <ll::concepts::ArrayLike T>
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& arr, ll::meta::PriorityTag<1>) {
    if (!dv.is_array()) return error_utils::makeFromDynamicTypeError<T, ArrayType>(dv);
    using value_type = typename T::value_type;
    if constexpr (requires(T a) { a.clear(); }) {
        arr.clear();
    }
    if constexpr (requires(T a, value_type v) { a.push_back(v); }) {
        for (size_t i = 0; i < dv.size(); i++) {
            if (auto res = detail::fromDynamicInternal<value_type>(dv[i], arr.emplace_back()); !res) {
                res = error_utils::makeSerIndexError(i, res.error());
                return res;
            }
        }
    } else if constexpr (requires(T a, value_type v) { a.insert(v); }) {
        for (size_t i = 0; i < dv.size(); i++) {
            value_type tmp{};
            if (auto res = detail::fromDynamicInternal<value_type>(dv[i], tmp); !res) {
                res = error_utils::makeSerIndexError(i, res.error());
                return res;
            }
            arr.insert(std::move(tmp));
        }
    } else static_assert(false, "Unsupported Type");
    return {};
}

// dict
template <class T>
    requires(ll::concepts::Associative<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& map, ll::meta::PriorityTag<1>) {
    using RT = std::remove_cvref_t<T>;
    static_assert(
        (concepts::IsString<typename RT::key_type> || std::is_enum_v<typename RT::key_type>),
        "the key type of the associative container must be convertible to a string"
    );
    dv                 = DynamicValue::object();
    ll::Expected<> res = {};
    for (auto&& [k, v] : map) {
        std::string key;
        if constexpr (std::is_enum_v<typename RT::key_type>) {
            key = magic_enum::enum_name(std::forward<decltype((k))>(k));
        } else {
            key = std::string{std::forward<decltype((k))>(k)};
        }
        res = detail::toDynamicInternal(dv[key], std::forward<decltype((v))>(v));
        if (!res) {
            res = error_utils::makeSerKeyError(key, res.error());
            break;
        }
    }
    if (dv.size() != map.size()) __debugbreak();
    return res;
}
template <ll::concepts::Associative T>
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& map, ll::meta::PriorityTag<1>) {
    static_assert(
        (concepts::IsString<typename T::key_type> || std::is_enum_v<typename T::key_type>),
        "the key type of the associative container must be convertible "
        "to a string"
    );
    if (!dv.is_object()) return error_utils::makeFromDynamicTypeError<T, ObjectType>(dv);
    map.clear();
    for (auto&& [k, v] : dv.items()) {
        if constexpr (std::is_enum_v<typename T::key_type>) {
            if (auto res = detail::fromDynamicInternal<typename T::mapped_type>(
                    dv,
                    map[magic_enum::enum_cast<typename T::key_type>(k).value()]
                );
                !res) {
                res = error_utils::makeSerKeyError(magic_enum::enum_cast<typename T::key_type>(k).value(), res.error());
                return res;
            }
        } else {
            if (auto res = detail::fromDynamicInternal<typename T::mapped_type>(v, map[k]); !res) {
                res = error_utils::makeSerKeyError(k, res.error());
                return res;
            }
        }
    }
    return {};
}

// object
template <class T>
    requires(ll::reflection::Reflectable<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& obj, ll::meta::PriorityTag<1>) {
    ll::Expected<> res = {};
    dv                 = DynamicValue::object();
    reflection::forEachMember(obj, [&](std::string_view name, auto&& member) {
        if (name.starts_with('$') || !res) {
            return;
        }
        using member_type = decltype((member));
        if constexpr (requires(member_type m) { detail::toDynamicInternal(dv, std::forward<member_type>(m)); }) {
            res = detail::toDynamicInternal(dv[name], std::forward<member_type>(member));
            if (!res) {
                res = error_utils::makeSerMemberError(std::string{name}, res.error());
            }
        } else {
            res = error_utils::makeSerMemberError(std::string{name}, res.error());
            static_assert(ll::traits::always_false<member_type>, "this type can't serialize");
        }
    });
    return res;
}
template <ll::reflection::Reflectable T>
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& obj, ll::meta::PriorityTag<1>) {
    if (!dv.is_object()) return error_utils::makeFromDynamicTypeError<T, ObjectType>(dv);
    ll::Expected<> res;
    reflection::forEachMember(obj, [&](std::string_view name, auto& member) {
        if (name.starts_with('$') || !res) {
            return;
        }
        using member_type = std::remove_cvref_t<decltype((member))>;
        auto sname        = std::string{name};
        if (dv.contains(sname)) {
            if constexpr (requires(DynamicValue& s, member_type& o) {
                              detail::fromDynamicInternal<member_type>(s, o);
                          }) {
                res = detail::fromDynamicInternal<member_type>(dv[sname], member);
                if (!res) res = error_utils::makeSerMemberError(sname, res.error());
            } else {
                res = error_utils::makeSerMemberError(sname, res.error());
                static_assert(ll::traits::always_false<member_type>, "this type can't deserialize");
            }
        } else {
            if constexpr (!ll::concepts::IsOptional<member_type>) {
                res = error_utils::makeError(
                    error_utils::ErrorReason::KeyNotFound,
                    fmt::format("missing required field \"{}\" when deserializing", sname)
                );
            } else {
                member = std::nullopt;
            }
        }
    });
    return res;
}

// enum
template <class T>
    requires(std::is_enum_v<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& e, ll::meta::PriorityTag<1>) {
    dv = magic_enum::enum_name(std::forward<T>(e));
    return {};
}
template <ll::concepts::Require<std::is_enum> T>
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& e, ll::meta::PriorityTag<1>) {
    using enum_type = std::remove_cvref_t<T>;
    if (dv.is_string()) {
        e = magic_enum::enum_cast<enum_type>(std::string{dv}).value();
    } else {
        e = magic_enum::enum_cast<enum_type>(std::underlying_type_t<enum_type>{dv}).value();
    }
    return {};
}

#if false

namespace concepts {
template <typename T>
concept IsPointer = std::is_pointer_v<T>;

template <typename T>
concept IsOptionalRef = requires(std::decay_t<T> v) {
    { v.as_ptr() } -> IsPointer;
    v.has_value();
    v = v.as_ptr();
};
static_assert(IsOptionalRef<optional_ref<Actor>>);
} // namespace concepts

// Error
// optional_ref;
template <concepts::IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
inline ll::Expected<> toDynamicImpl(DynamicValue& dv, T&& t, priority::LowTag) {
    if (t.has_value()) return detail::toDynamicInternal(dv, std::forward<T>(t).as_ptr());
    else dv.emplace<ElementType>(NullValue);
    return {};
}
template <concepts::IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
inline ll::Expected<> fromDynamicImpl(DynamicValue& dv, T& t, priority::DefaultTag) {
    if (dv.hold<NullType>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = detail::fromDynamicInternal(dv, val);
        if (res) t = val;
        return res;
    }
}
#endif

} // namespace remote_call