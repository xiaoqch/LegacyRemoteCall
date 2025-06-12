#pragma once

#include "fmt/core.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "ll/api/reflection/Reflection.h"
#include "magic_enum.hpp"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/reflection/Reflection.h"
#include "remote_call/api/reflection/TypeName.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/Base.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call::detail {

// std::optional<T>
template <typename T>
    requires(ll::concepts::IsOptional<std::remove_cvref_t<T>> && concepts::SupportToDynamic<typename std::remove_cvref_t<T>::value_type>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::HightTag) {
    if (t.has_value()) return ::remote_call::toDynamic(dv, *std::forward<T>(t));
    dv.emplace<NullType>(NULL_VALUE);
    return {};
}
template <ll::concepts::IsOptional T>
    requires(concepts::SupportFromDynamic<typename T::value_type> && std::is_default_constructible_v<typename T::value_type>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::HightTag) {
    using ValueType = T::value_type;
    if (dv.hold<NullType>()) {
        t.reset();
        return {};
    } else {
        return dv.tryGet<ValueType>().and_then([&](ValueType&& val) -> ll::Expected<> {
            t.emplace(std::move(val));
            return {};
        });
    }
}
template <ll::concepts::IsOptional T>
    requires(concepts::SupportFromDynamic<typename std::remove_cvref_t<T>::value_type> && !std::is_default_constructible_v<typename std::remove_cvref_t<T>::value_type>)
LL_CONSTEXPR23 ll::Expected<T> fromDynamic(DynamicValue& dv, std::in_place_type_t<T>, priority::HightTag) {
    using ValueType = std::remove_cvref_t<T>::value_type;
    if (dv.hold<NullType>()) {
        return {};
    } else {
        return dv.tryGet<ValueType>().transform([&](ValueType&& val) {
            return std::make_optional<ValueType>(std::move(val));
        });
    }
}

// Vec3, BlockPos, Vec2...
template <class T>
    requires(ll::concepts::IsVectorBase<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& vec, ll::meta::PriorityTag<2>) {
    dv                 = DynamicValue::array();
    ll::Expected<> res = {};
    std::remove_cvref_t<T>::forEachComponent([&]<typename axis_type, size_t iter> {
        if (res) {
            res = ::remote_call::toDynamic(dv.emplace_back(), std::forward<T>(vec).template get<axis_type, iter>());
            if (!res) res = error_utils::makeSerIndexError(iter, res.error());
        }
    });
    return res;
}
template <ll::concepts::IsVectorBase T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& vec, ll::meta::PriorityTag<2>) {
    ll::Expected<> res{};
    T::forEachComponent([&]<typename axis_type, size_t iter> {
        if (res) {
            res = ::remote_call::fromDynamic(dv[iter], vec.template get<axis_type, iter>());
            if (!res) res = error_utils::makeSerIndexError(iter, res.error());
        }
    });
    return res;
}

// tuple, array
template <class T>
    requires(concepts::IsTupleLike<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& tuple, ll::meta::PriorityTag<2>) {
    ll::Expected<> res  = {};
    dv                  = DynamicValue::array();
    constexpr auto impl = [](auto& ji, auto&& ti, auto& res, size_t i) {
        if (!res) return;
        res = ::remote_call::toDynamic(ji, std::forward<decltype(ti)>(ti));
        if (!res) res = error_utils::makeSerIndexError(i, res.error());
    };
    (void)[&]<int... I>(T && t, std::index_sequence<I...>) {
        (void)(impl(dv.emplace_back(), std::get<I>(std::forward<T>(t)), res, I), ...);
    }
    (std::forward<T>(tuple), std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{});
    return res;
}
template <concepts::IsTupleLike T>
    requires(std::is_default_constructible_v<T>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& tuple, ll::meta::PriorityTag<2>) {
    if (!dv.is_array()) return error_utils::makeFromDynamicTypeError<T, DynamicArray>(dv);
    if (dv.size() != std::tuple_size_v<T>)
        return error_utils::makeError(
            error_utils::ErrorReason::IndexOutOfRange,
            fmt::format("array size must be {}", std::tuple_size_v<T>)
        );
    ll::Expected<> res{};

    constexpr auto func = [](auto& dv, auto& ti, auto& res, size_t i) {
        if (!res) return;
        res = ::remote_call::fromDynamic(dv, ti);
        if (!res) res = error_utils::makeSerIndexError(i, res.error());
    };
    (void)[&]<int... I>(std::index_sequence<I...>) { (void)(func(dv[I], std::get<I>(tuple), res, I), ...); }
    (std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{});
    return res;
}
template <typename Tpl, size_t... I>
ll::Expected<Tpl> fromDynamicTupleImpl(DynamicValue& dv, std::index_sequence<I...>) {
    ll::Expected<> res;
    constexpr auto getElement =
        []<size_t Idx, typename TupleElement>(DynamicValue& dv, ll::Expected<>& res) -> std::optional<TupleElement> {
        if (!res) return std::nullopt;
        if (dv.size() > Idx) {
            if constexpr (requires() { dv[Idx].template tryGet<TupleElement>(res); }) {
                auto opt = dv[Idx].template tryGet<TupleElement>(res);
                if (!res) res = error_utils::makeSerIndexError(Idx, res.error());
                return opt;
            } else {
                __debugbreak();
                static_assert(ll::traits::always_false<TupleElement>, "this type can't deserialize");
            }
        } else {
            if constexpr (!ll::concepts::IsOptional<TupleElement>) {
                res = error_utils::makeError(
                    error_utils::ErrorReason::IndexOutOfRange,
                    fmt::format("index \"{}\" outof range", Idx)
                );
            } else {
                return std::make_optional<TupleElement>(std::nullopt);
            }
        }
        if (!res) res = error_utils::makeSerIndexError(Idx, res.error());
        return std::nullopt;
    };
    auto tmp = std::make_tuple(getElement.template operator()<I, std::tuple_element_t<I, Tpl>>(dv, res)...);
    if (res) return Tpl{*std::get<I>(std::move(tmp))...};
    return ll::forwardError(res.error());
}

static_assert(std::same_as<std::tuple_element_t<5, std::array<int, 11>>, int>);
template <concepts::IsTupleLike T>
    requires(!std::is_default_constructible_v<T>)
ll::Expected<T> fromDynamic(DynamicValue& dv, std::in_place_type_t<T>, ll::meta::PriorityTag<2>) {
    using TupleType = std::decay_t<T>;
    return fromDynamicTupleImpl<TupleType>(dv, std::make_index_sequence<std::tuple_size_v<TupleType>>{});
}

static_assert(!ll::concepts::ArrayLike<std::tuple<int>>);
static_assert(ll::concepts::ArrayLike<std::vector<int>>);

// vector
template <class T>
    requires(ll::concepts::ArrayLike<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& arr, ll::meta::PriorityTag<1>) {
    ll::Expected<> res = {};
    dv                 = DynamicValue::array();
    size_t iter{0};
    for (auto&& val : std::forward<T>(arr)) {
        if (res) {
            if constexpr (std::is_lvalue_reference_v<decltype(arr)>) {
                res = ::remote_call::toDynamic(dv.emplace_back(), val);
            } else {
                res = ::remote_call::toDynamic(dv.emplace_back(), std::move(val));
            }
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
    requires(requires() { typename T::value_type; })
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& arr, ll::meta::PriorityTag<1>) {
    if (!dv.is_array()) return error_utils::makeFromDynamicTypeError<T, DynamicArray>(dv);
    using value_type = typename T::value_type;
    if constexpr (requires() { arr.clear(); }) {
        arr.clear();
    }
    for (size_t i = 0; i < dv.size(); i++) {
        auto res = dv[i].tryGet<value_type>();
        if (!res) return error_utils::makeSerIndexError(i, res.error());

        if constexpr (requires(value_type v) { arr.emplace_back(std::forward<value_type>(v)); }) {
            arr.emplace_back(std::move(*res));
        } else if constexpr (requires(value_type v) { arr.push_back(std::forward<value_type>(v)); }) {
            arr.push_back(std::move(*res));
        } else if constexpr (requires(value_type v) { arr.emplace(v); }) {
            arr.emplace(*res);
        } else if constexpr (requires(value_type v) { arr.insert(std::forward<value_type>(v)); }) {
            arr.insert(std::move(*res));
        } else {
            __debugbreak();
            static_assert(ll::traits::always_false<value_type>, "this type can't deserialize");
        }
    }
    return {};
}

// dict
template <class T>
    requires(ll::concepts::Associative<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& map, ll::meta::PriorityTag<1>) {
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
            key = magic_enum::enum_name(std::forward<decltype(k)>(k));
        } else {
            key = std::string{std::forward<decltype(k)>(k)};
        }
        res = ::remote_call::toDynamic(dv[key], std::forward<decltype(v)>(v));
        if (!res) {
            res = error_utils::makeSerKeyError(key, res.error());
            break;
        }
    }
    if (dv.size() != map.size()) __debugbreak();
    return res;
}
template <ll::concepts::Associative T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& map, ll::meta::PriorityTag<1>) {
    static_assert(
        (concepts::IsString<typename T::key_type> || std::is_enum_v<typename T::key_type>),
        "the key type of the associative container must be convertible "
        "to a string"
    );
    if (!dv.is_object()) return error_utils::makeFromDynamicTypeError<T, DynamicObject>(dv);
    map.clear();
    for (auto&& [k, v] : dv.items()) {
        if constexpr (std::is_enum_v<typename T::key_type>) {
            auto key = magic_enum::enum_cast<typename T::key_type>(k);
            if (!key)
                return error_utils::makeError(
                    error_utils::ErrorReason::UnsupportedValue,
                    fmt::format("enum key '{}' cannot parse as '{}'", k, reflection::typeName<typename T::key_type>())
                );
            auto res = v.tryGet<typename T::mapped_type>();
            if (!res) return error_utils::makeSerKeyError(k, res.error());
            map.try_emplace(*key, *std::move(res));
        } else {
            if (auto res = v.tryGet<typename T::mapped_type>()) {
                map.try_emplace(k, *std::move(res));
            } else {
                return error_utils::makeSerKeyError(k, res.error());
            }
        }
    }
    return {};
}

// object
template <class T>
    requires(ll::reflection::Reflectable<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& obj, ll::meta::PriorityTag<1>) {
    ll::Expected<> res = {};
    dv                 = DynamicValue::object();
    reflection::forEachMember(std::forward<T>(obj), [&](std::string_view name, auto&& member) {
        if (name.starts_with('$') || !res) {
            return;
        }
        using member_type = decltype(member);
        if constexpr (requires(member_type m) { ::remote_call::toDynamic(dv, std::forward<member_type>(m)); }) {
            res = ::remote_call::toDynamic(dv[name], std::forward<member_type>(member));
            if (!res) {
                res = error_utils::makeSerMemberError(std::string{name}, res.error());
            }
        } else {
            __debugbreak();
            static_assert(ll::traits::always_false<member_type>, "this type can't serialize");
        }
    });
    return res;
}

template <ll::reflection::Reflectable T>
    requires(std::is_default_constructible_v<T>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& obj, ll::meta::PriorityTag<1>) {
    if (!dv.is_object()) return error_utils::makeFromDynamicTypeError<T, DynamicObject>(dv);
    ll::Expected<> res;
    reflection::forEachMember(obj, [&](std::string_view name, auto& member) {
        if (name.starts_with('$') || !res) {
            return;
        }
        using member_type = std::remove_cvref_t<decltype(member)>;
        auto sname        = std::string{name};
        if (dv.contains(sname)) {
            if constexpr (requires() { dv.getTo(member); }) {
                res = dv[sname].getTo(member);
                if (!res) res = error_utils::makeSerMemberError(sname, res.error());
            } else if constexpr (requires() { dv.tryGet<std::decay_t<decltype(member)>>(res); }) {
                auto opt = dv.tryGet<std::decay_t<decltype(member)>>(res);
                if (res) member = *std::move(opt);
            } else {
                __debugbreak();
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
template <typename Obj, typename... T, size_t... N>
    requires(sizeof...(T) == sizeof...(N))
ll::Expected<Obj>
fromDynamicReflectableImpl(DynamicValue& dv, std::in_place_type_t<boost::pfr::detail::sequence_tuple::tuple<T&...>>, std::index_sequence<N...>) {
    ll::Expected<> res;
    const auto     getMember =
        [&]<size_t I, typename MemberType>(std::in_place_index_t<I>, std::in_place_type_t<MemberType>)
        -> std::optional<MemberType> {
        if (!res) return std::nullopt;
        constexpr std::string sname = std::string{ll::reflection::member_name_array_v<Obj>[I]};
        if (dv.contains(sname)) {
            if constexpr (requires(DynamicValue& s, MemberType& o) { std::move(s).getTo(o); }) {
                auto opt = dv[sname].template tryGet<MemberType>(res);
                if (!res) res = error_utils::makeSerMemberError(sname, res.error());
                return opt;
            } else {
                __debugbreak();
                static_assert(ll::traits::always_false<MemberType>, "this type can't deserialize");
            }
        } else {
            if constexpr (!ll::concepts::IsOptional<MemberType>) {
                res = error_utils::makeError(
                    error_utils::ErrorReason::KeyNotFound,
                    fmt::format("missing required field \"{}\" when deserializing", sname)
                );
            } else {
                return std::make_optional(std::in_place, std::nullopt);
            }
        }
        if (!res) res = error_utils::makeSerMemberError(sname, res.error());
        return std::nullopt;
    };
    auto tmp = std::make_tuple(getMember(std::in_place_index<N>, std::in_place_type<T>)...);
    if (res) return Obj(*std::get<N>(std::move(tmp))...);
    return ll::forwardError(res.error());
}

template <ll::reflection::Reflectable T>
    requires(!std::is_default_constructible_v<T>)
ll::Expected<T> fromDynamic(DynamicValue& dv, std::in_place_type_t<T>, ll::meta::PriorityTag<1>) {
    if (!dv.is_object()) return error_utils::makeFromDynamicTypeError<T, DynamicObject>(dv);
    constexpr size_t size = ll::reflection::member_count_v<T>;
    using TupleType       = std::decay_t<decltype(boost::pfr::detail::tie_as_tuple(std::declval<T&>()))>;
    return fromDynamicReflectableImpl<T>(dv, std::in_place_type<TupleType>, std::make_index_sequence<size>{});
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
LL_CONSTEXPR23 ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::LowTag) {
    if (t.has_value()) return ::remote_call::toDynamic(dv, std::forward<T>(t).as_ptr());
    else dv.emplace<NullType>(NullValue);
    return {};
}
template <concepts::IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
LL_CONSTEXPR23 ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::DefaultTag) {
    if (dv.hold<NullType>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = ::remote_call::fromDynamic(dv, val);
        if (res) t = val;
        return res;
    }
}

#endif

} // namespace remote_call::detail