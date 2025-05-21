#pragma once

#include "remote_call/api/base/Concepts.h"
#include "ll/api/Expected.h"
#include "remote_call/api/base/Meta.h"

namespace remote_call::detail {
using namespace remote_call::concepts;

template <typename T>
void toValue(DynamicValue&, T&&, PriorityTag<0>) = delete;
template <typename T>
void fromValue(DynamicValue&, T&, PriorityTag<0>) = delete;

template <SupportToValue T>
[[nodiscard]] inline ll::Expected<> toValueInternal(DynamicValue& v, T&& val) {
    if constexpr (SupportToValueWithPrio<T>) {
        using Ret = decltype(toValue(v, std::forward<T>(val), priority::Hightest));
        if constexpr (IsLeviExpected<Ret>) {
            return toValue(v, std::forward<T>(val), priority::Hightest);
        } else {
            toValue(v, std::forward<T>(val), priority::Hightest);
            return {};
        }
    } else if constexpr (SupportToValue<T>) {
        using Ret = decltype(toValue(v, std::forward<T>(val)));
        if constexpr (IsLeviExpected<Ret>) {
            return toValue(v, std::forward<T>(val));
        } else {
            toValue(v, std::forward<T>(val));
            return {};
        }
    } else static_assert(false, "Can not found toValue for type");
};

template <SupportFromValue T>
[[nodiscard]] inline ll::Expected<> fromValueInternal(DynamicValue& v, T& t) {
    if constexpr (SupportFromValueWithPrio<T>) {
        using Ret = decltype(fromValue(v, t, priority::Hightest));
        if constexpr (IsLeviExpected<Ret>) {
            return fromValue(v, t, priority::Hightest);
        } else {
            fromValue(v, t, priority::Hightest);
            return {};
        }
    } else if constexpr (SupportFromValue<T>) {
        using Ret = decltype(fromValue(v, t));
        if constexpr (IsLeviExpected<Ret>) {
            return fromValue(v, t);
        } else {
            fromValue(v, t);
            return {};
        }
    } else static_assert(false, "Can not found fromValue for type");
};

} // namespace remote_call::detail