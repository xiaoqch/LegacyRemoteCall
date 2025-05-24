#pragma once

#include "ll/api/Expected.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"


namespace remote_call::detail {
using namespace remote_call::concepts;

template <typename T>
void toDynamicImpl(DynamicValue&, T&&, PriorityTag<0>) = delete;
template <typename T>
void fromDynamicImpl(DynamicValue&, T&, PriorityTag<0>) = delete;

template <SupportToDynamic T>
[[nodiscard]] inline ll::Expected<> toDynamicInternal(DynamicValue& dv, T&& val) {
    if constexpr (SupportToDynamicWithPrio<T>) {
        using Ret = decltype(toDynamicImpl(dv, std::forward<T>(val), priority::Hightest));
        if constexpr (IsLeviExpected<Ret>) {
            return toDynamicImpl(dv, std::forward<T>(val), priority::Hightest);
        } else {
            toDynamicImpl(dv, std::forward<T>(val), priority::Hightest);
            return {};
        }
    } else if constexpr (SupportToDynamic<T>) {
        using Ret = decltype(toDynamicImpl(dv, std::forward<T>(val)));
        if constexpr (IsLeviExpected<Ret>) {
            return toDynamicImpl(dv, std::forward<T>(val));
        } else {
            toDynamicImpl(dv, std::forward<T>(val));
            return {};
        }
    } else static_assert(false, "Can not found toDynamicImpl for type");
};

template <SupportFromDynamic T>
[[nodiscard]] inline ll::Expected<> fromDynamicInternal(DynamicValue& dv, T& t) {
    if constexpr (SupportFromDynamicWithPrio<T>) {
        using Ret = decltype(fromDynamicImpl(dv, t, priority::Hightest));
        if constexpr (IsLeviExpected<Ret>) {
            return fromDynamicImpl(dv, t, priority::Hightest);
        } else {
            fromDynamicImpl(dv, t, priority::Hightest);
            return {};
        }
    } else if constexpr (SupportFromDynamic<T>) {
        using Ret = decltype(fromDynamicImpl(dv, t));
        if constexpr (IsLeviExpected<Ret>) {
            return fromDynamicImpl(dv, t);
        } else {
            fromDynamicImpl(dv, t);
            return {};
        }
    } else static_assert(false, "Can not found fromDynamicImpl for type");
};

} // namespace remote_call::detail