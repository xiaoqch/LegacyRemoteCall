// Modified from
// https://github.com/LiteLDev/LeviLamina/blob/18170e92c47f21a54378379c542a819533b7ab43/src/ll/api/reflection/Deserialization.h
#pragma once

#include "Reflection.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Meta.h"
#include "ll/api/reflection/Reflection.h"
#include "ll/api/reflection/SerializationError.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/utils/ErrorUtils.h"

// Priority:
// 6. Custom deserializeImpl
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

template <ll::concepts::IsVectorBase T, class J>
inline Expected<> deserializeImpl(J& j, T& vec, ll::meta::PriorityTag<5>);
template <ll::concepts::IsDispatcher T, class J>
inline Expected<> deserializeImpl(J& j, T& d, ll::meta::PriorityTag<5>);
template <ll::concepts::IsOptional T, class J>
inline Expected<> deserializeImpl(J& j, T& opt, ll::meta::PriorityTag<5>);
template <concepts::IsString T, class J>
    requires(!std::same_as<T, nullptr_t>)
inline Expected<> deserializeImpl(J& j, T& str, ll::meta::PriorityTag<4>);
template <concepts::IsTupleLike T, class J>
inline Expected<> deserializeImpl(J& j, T& tuple, ll::meta::PriorityTag<3>);
template <ll::concepts::ArrayLike T, class J>
inline Expected<> deserializeImpl(J& j, T& arr, ll::meta::PriorityTag<2>);
template <ll::concepts::Associative T, class J>
inline Expected<> deserializeImpl(J& j, T& map, ll::meta::PriorityTag<2>);
template <Reflectable T, class J>
inline Expected<> deserializeImpl(J& j, T& obj, ll::meta::PriorityTag<1>);
template <ll::concepts::Require<std::is_enum> T, class J>
inline Expected<> deserializeImpl(J& j, T& e, ll::meta::PriorityTag<1>);
template <class T, class J>
inline Expected<> deserializeImpl(J& j, T& obj, ll::meta::PriorityTag<0>)
    requires(std::convertible_to<J, T> && !concepts::IsDynamicValue<J>);

template <class T, class J>
[[nodiscard]] inline Expected<> deserialize_to(J& j, T& t) noexcept
#if !defined(__INTELLISENSE__)
    requires(requires(J& j, T& t) { deserializeImpl(j, t, priority::Hightest); })
#endif
try {
    return deserializeImpl(j, t, priority::Hightest);
} catch (...) {
    return ll::makeExceptionError();
}

template <std::default_initializable T, class J>
    requires(std::is_default_constructible_v<std::remove_reference_t<T>>)
[[nodiscard]] inline Expected<T> deserialize(J&& j) noexcept {
    J           tmp = std::forward<J>(j);
    Expected<T> res{};
    if (auto d = deserialize_to<T>(tmp, *res); !d) {
        res = ll::forwardError(d.error());
    }
    return res;
}


template <ll::concepts::IsVectorBase T, class J>
inline Expected<> deserializeImpl(J& j, T& vec, ll::meta::PriorityTag<5>) {
    Expected<> res{};
    T::forEachComponent([&]<typename axis_type, size_t iter> {
        if (res) {
            res = deserialize_to<axis_type>(j[iter], vec.template get<axis_type, iter>());
            if (!res) res = error_utils::makeSerIndexError(iter, res.error());
        }
    });
    return res;
}
template <ll::concepts::IsDispatcher T, class J>
inline Expected<> deserializeImpl(J& j, T& d, ll::meta::PriorityTag<5>) {
    auto res{deserialize_to<typename T::storage_type>(j, d.storage)};
    if (res) d.call();
    return res;
}
template <ll::concepts::IsOptional T, class J>
inline Expected<> deserializeImpl(J& j, T& opt, ll::meta::PriorityTag<5>) {
    Expected<> res;
    if (j.is_null()) {
        opt = std::nullopt;
    } else {
        if (!opt) opt.emplace();
        res = deserialize_to<typename T::value_type>(j, *opt);
    }
    return res;
}
template <concepts::IsString T, class J>
    requires(!std::same_as<T, nullptr_t>)
inline Expected<> deserializeImpl(J& j, T& str, ll::meta::PriorityTag<4>) {
    if (!j.is_string()) return error_utils::makeError(error_utils::ErrorReason::UnexpectedType,"field must be a string");
    str = std::forward<J>(j);
    return {};
}
template <concepts::IsTupleLike T, class J>
inline Expected<> deserializeImpl(J& j, T& tuple, ll::meta::PriorityTag<3>) {
    if (!j.is_array()) return error_utils::makeError(error_utils::ErrorReason::UnexpectedType,"field must be an array");
    if (j.size() != std::tuple_size_v<T>)
        return error_utils::makeError(error_utils::ErrorReason::IndexOutOfRange,fmt::format("array size must be {}", std::tuple_size_v<T>));
    Expected<> res{};

    constexpr auto func = [](auto& ji, auto& ti, auto& res, size_t i) {
        if (!res) return;
        res = deserialize_to(ji, ti);
        if (!res) res = error_utils::makeSerIndexError(i, res.error());
    };
    (void)[&]<int... I>(std::index_sequence<I...>) { (void)(func(j[I], std::get<I>(tuple), res, I), ...); }
    (std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{});
    return res;
}
template <ll::concepts::ArrayLike T, class J>
inline Expected<> deserializeImpl(J& j, T& arr, ll::meta::PriorityTag<2>) {
    if (!j.is_array()) return error_utils::makeError(error_utils::ErrorReason::UnexpectedType,"field must be an array");
    using value_type = typename T::value_type;
    if constexpr (requires(T a) { a.clear(); }) {
        arr.clear();
    }
    if constexpr (requires(T a, value_type v) { a.push_back(v); }) {
        for (size_t i = 0; i < j.size(); i++) {
            if (auto res = deserialize_to<value_type>(j[i], arr.emplace_back()); !res) {
                res = error_utils::makeSerIndexError(i, res.error());
                return res;
            }
        }
    } else if constexpr (requires(T a, value_type v) { a.insert(v); }) {
        for (size_t i = 0; i < j.size(); i++) {
            value_type tmp{};
            if (auto res = deserialize_to<value_type>(j[i], tmp); !res) {
                res = error_utils::makeSerIndexError(i, res.error());
                return res;
            }
            arr.insert(std::move(tmp));
        }
    } else static_assert(false, "Unsupported Type");
    return {};
}
template <ll::concepts::Associative T, class J>
inline Expected<> deserializeImpl(J& j, T& map, ll::meta::PriorityTag<2>) {
    static_assert(
        (concepts::IsString<typename T::key_type> || std::is_enum_v<typename T::key_type>),
        "the key type of the associative container must be convertible "
        "to a string"
    );
    if (!j.is_object()) return error_utils::makeError(error_utils::ErrorReason::KeyNotFound,"field must be an object");
    map.clear();
    for (auto&& [k, v] : j.items()) {
        if constexpr (std::is_enum_v<typename T::key_type>) {
            if (auto res = deserialize_to<typename T::mapped_type>(
                    j,
                    map[magic_enum::enum_cast<typename T::key_type>(k).value()]
                );
                !res) {
                res = error_utils::makeSerKeyError(magic_enum::enum_cast<typename T::key_type>(k).value(), res.error());
                return res;
            }
        } else {
            if (auto res = deserialize_to<typename T::mapped_type>(v, map[k]); !res) {
                res = error_utils::makeSerKeyError(k, res.error());
                return res;
            }
        }
    }
    return {};
}

template <Reflectable T, class J>
inline Expected<> deserializeImpl(J& j, T& obj, ll::meta::PriorityTag<1>) {
    Expected<> res;
    if (!j.is_object()) {
        res = error_utils::makeError(error_utils::ErrorReason::UnexpectedType,"field must be an object");
        return res;
    }
    reflection::forEachMember(obj, [&](std::string_view name, auto& member) {
        if (name.starts_with('$') || !res) {
            return;
        }
        using member_type = std::remove_cvref_t<decltype((member))>;
        auto sname        = std::string{name};
        if (j.contains(sname)) {
            if constexpr (requires(member_type& o, J& s) { deserialize_to<member_type>(s, o); }) {
                res = deserialize_to<member_type>(j[sname], member);
                if (!res) res = error_utils::makeSerMemberError(sname, res.error());
            } else {
                static_assert(ll::traits::always_false<member_type>, "this type can't deserialize");
            }
        } else {
            if constexpr (!ll::concepts::IsOptional<member_type>) {
                res = error_utils::makeError(error_utils::ErrorReason::KeyNotFound,fmt::format("missing required field \"{}\" when deserializing", sname));
            } else {
                member = std::nullopt;
            }
        }
    });
    return res;
}
template <ll::concepts::Require<std::is_enum> T, class J>
inline Expected<> deserializeImpl(J& j, T& e, ll::meta::PriorityTag<1>) {
    using enum_type = std::remove_cvref_t<T>;
    if (j.is_string()) {
        e = magic_enum::enum_cast<enum_type>(std::string{j}).value();
    } else {
        e = magic_enum::enum_cast<enum_type>(std::underlying_type_t<enum_type>{j}).value();
    }
    return {};
}


template <class T, class J>
inline Expected<> deserializeImpl(J& j, T& obj, ll::meta::PriorityTag<0>)
    requires(std::convertible_to<J, T> && !concepts::IsDynamicValue<J>)
{
    obj = j;
    return {};
}
} // namespace remote_call::reflection
