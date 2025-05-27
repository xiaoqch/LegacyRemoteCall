#pragma once

#include "ll/api/reflection/Reflection.h"

namespace remote_call::reflection {

// Keep rvalue reference
template <ll::reflection::Reflectable T, class F>
constexpr void forEachMember(T&& value, F&& func) {
    boost::pfr::for_each_field(std::forward<T>(value), [func = std::forward<F>(func)](auto&& field, std::size_t idx) {
        if constexpr (std::is_rvalue_reference_v<T&&>)
            func(
                ll::reflection::member_name_array_v<T>[idx],
                std::move(field) // NOLINT: bugprone-move-forwarding-reference
            );
        else func(ll::reflection::member_name_array_v<T>[idx], std::forward<decltype(field)>(field));
    });
}

} // namespace remote_call::reflection