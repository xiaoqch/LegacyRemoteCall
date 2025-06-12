#pragma once

#include "ll/api/base/Meta.h"

namespace remote_call::priority {

using ll::meta::PriorityTag;
using HightestTag = PriorityTag<10>;
using HightTag    = PriorityTag<7>;
using DefaultTag  = PriorityTag<5>;
using LowTag      = PriorityTag<3>;

template <size_t P = 10>
inline constexpr auto Priority = PriorityTag<P>{};

inline constexpr auto Hightest = Priority<10>;

} // namespace remote_call::priority
