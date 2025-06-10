#pragma once

#include "ll/api/base/Concepts.h"
#include "remote_call/api/conversions/detail/Detail.h"

namespace remote_call {

template <typename ValueType>
struct AdlSerializer {
    template <typename TargetType = ValueType>
    [[nodiscard]] inline static auto
    fromDynamic(DynamicValue& dv, TargetType& val) noexcept(noexcept(::remote_call::fromDynamic(dv, val)))
        -> decltype(::remote_call::fromDynamic(dv, val)) {
        return ::remote_call::fromDynamic(dv, val);
    }

    template <typename TargetType = ValueType>
    [[nodiscard]] inline static auto fromDynamic(DynamicValue& dv
    ) noexcept(noexcept(::remote_call::fromDynamic(dv, std::in_place_type<TargetType>)))
        -> decltype(::remote_call::fromDynamic(dv, std::in_place_type<TargetType>)) {
        return ::remote_call::fromDynamic(dv, std::in_place_type<TargetType>);
    }

    template <typename TargetType = ValueType>
    [[nodiscard]] inline static auto toDynamic(DynamicValue& dv, TargetType&& val) noexcept(
        noexcept(::remote_call::toDynamic(dv, std::forward<TargetType>(val)))
    ) -> decltype(::remote_call::toDynamic(dv, std::forward<TargetType>(val))) {
        return ::remote_call::toDynamic(dv, std::forward<TargetType>(val));
    }
};

namespace concepts {
template <typename T>
concept SupportToDynamic =
    requires(DynamicValue& dv, T&& val) { AdlSerializer<T>::toDynamic(dv, std::forward<T>(val)); };


template <typename T>
concept SupportFromDynamicR = requires(DynamicValue& dv, std::remove_cvref_t<T>& t) {
    AdlSerializer<std::remove_cvref_t<T>>::fromDynamic(dv, t);
};
template <typename T>
concept SupportFromDynamicC = requires(DynamicValue& dv) {
    { AdlSerializer<T>::fromDynamic(dv) } -> ll::concepts::IsOneOf<T, ll::Expected<T>>;
};

template <typename T>
concept SupportFromDynamic = SupportFromDynamicC<T> || SupportFromDynamicR<T>;


} // namespace concepts

} // namespace remote_call
