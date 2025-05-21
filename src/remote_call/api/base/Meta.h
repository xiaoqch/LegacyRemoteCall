#pragma once

#include "ll/api/base/Meta.h"

namespace remote_call::priority {

using HightestTag = ll::meta::PriorityTag<10>;
using DefaultTag  = ll::meta::PriorityTag<5>;
using LowTag      = ll::meta::PriorityTag<3>;

template <size_t P>
constexpr auto Priority = ll::meta::PriorityTag<P>{};

constexpr auto Hightest = Priority<10>;

} // namespace remote_call::priority
