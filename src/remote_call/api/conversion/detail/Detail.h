#pragma once

#include "remote_call/api/base/Concepts.h"
#include "ll/api/Expected.h"
#include "remote_call/api/base/Meta.h"

namespace remote_call::detail {
using namespace remote_call::concepts;

template <typename T>
void toDynamic(DynamicValue&, T&&, PriorityTag<0>) = delete;
template <typename T>
void fromDynamic(DynamicValue&, T&, PriorityTag<0>) = delete;

template <SupportToDynamic T>
[[nodiscard]] inline ll::Expected<> toDynamicInternal(DynamicValue& v, T&& val) {
    if constexpr (SupportToDynamicWithPrio<T>) {
        using Ret = decltype(toDynamic(v, std::forward<T>(val), priority::Hightest));
        if constexpr (IsLeviExpected<Ret>) {
            return toDynamic(v, std::forward<T>(val), priority::Hightest);
        } else {
            toDynamic(v, std::forward<T>(val), priority::Hightest);
            return {};
        }
    } else if constexpr (SupportToDynamic<T>) {
        using Ret = decltype(toDynamic(v, std::forward<T>(val)));
        if constexpr (IsLeviExpected<Ret>) {
            return toDynamic(v, std::forward<T>(val));
        } else {
            toDynamic(v, std::forward<T>(val));
            return {};
        }
    } else static_assert(false, "Can not found toDynamic for type");
};

template <SupportFromDynamic T>
[[nodiscard]] inline ll::Expected<> fromDynamicInternal(DynamicValue& v, T& t) {
    if constexpr (SupportFromDynamicWithPrio<T>) {
        using Ret = decltype(fromDynamic(v, t, priority::Hightest));
        if constexpr (IsLeviExpected<Ret>) {
            return fromDynamic(v, t, priority::Hightest);
        } else {
            fromDynamic(v, t, priority::Hightest);
            return {};
        }
    } else if constexpr (SupportFromDynamic<T>) {
        using Ret = decltype(fromDynamic(v, t));
        if constexpr (IsLeviExpected<Ret>) {
            return fromDynamic(v, t);
        } else {
            fromDynamic(v, t);
            return {};
        }
    } else static_assert(false, "Can not found fromDynamic for type");
};

} // namespace remote_call::detail