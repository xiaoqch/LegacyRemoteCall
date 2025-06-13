#pragma once

#include "ll/api/Expected.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"


namespace remote_call {
namespace detail {

template <typename T>
void toDynamic(DynamicValue&, T&&, priority::PriorityTag<0>) = delete;
template <typename T>
void fromDynamic(DynamicValue&, T&, priority::PriorityTag<0>) = delete;

struct FromDynamicFn {
    template <typename T>
    [[nodiscard]] LL_FORCEINLINE constexpr auto operator()(DynamicValue& dv, T& t) const
        noexcept(noexcept(fromDynamic(dv, t, priority::Hightest)))
        requires(std::is_default_constructible_v<T>&&requires() {
            { fromDynamic(dv, t, priority::Hightest) } -> ll::concepts::IsLeviExpected;
        })
    {
        return fromDynamic(dv, t, priority::Hightest);
    }
    template <typename T>
    [[nodiscard]] LL_FORCEINLINE constexpr auto operator()(DynamicValue& dv) const
        noexcept(noexcept(fromDynamic(dv, std::in_place_type<T>, priority::Hightest)))
        requires(requires() {
            { fromDynamic(dv, std::in_place_type<T>, priority::Hightest) } -> ll::concepts::IsLeviExpected;
        })
    {
        return fromDynamic(dv, std::in_place_type<T>, priority::Hightest);
    }
};

struct ToDynamicFn {
    template <typename T>
    [[nodiscard]] constexpr auto operator()(DynamicValue& dv, T&& val) const
        noexcept(noexcept(toDynamic(dv, std::forward<T>(val), priority::Hightest)))
        requires(requires() {
            { toDynamic(dv, std::forward<T>(val), priority::Hightest) } -> ll::concepts::IsLeviExpected;
        })
    {
        return toDynamic(dv, std::forward<T>(val), priority::Hightest);
    }
};

} // namespace detail

inline constexpr const auto& fromDynamic = detail::static_const<detail::FromDynamicFn>::value;
inline constexpr const auto& toDynamic   = detail::static_const<detail::ToDynamicFn>::value;

namespace concepts {
template <typename T>
concept SupportToDynamic = requires(DynamicValue& dv, T&& val) { ::remote_call::toDynamic(dv, std::forward<T>(val)); };

template <typename T>
concept SupportFromDynamicR =
    requires(DynamicValue& dv, std::remove_cvref_t<T>& t) { ::remote_call::fromDynamic(dv, t); };
template <typename T>
concept SupportFromDynamicC = requires(DynamicValue& dv) {
    { ::remote_call::fromDynamic(dv, std::in_place_type<T>) } -> ll::concepts::IsOneOf<T, ll::Expected<T>>;
};

template <typename T>
concept SupportFromDynamic = SupportFromDynamicC<T> || SupportFromDynamicR<T>;

} // namespace concepts

} // namespace remote_call