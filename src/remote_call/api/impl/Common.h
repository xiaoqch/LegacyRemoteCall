#pragma once

#include "remote_call/api/base/Concepts.h"

namespace remote_call::impl {

template <typename Arg>
consteval void checkUptrType() {
    using DecayedArg = std::decay_t<Arg>;
    if constexpr (concepts::IsUniquePtr<DecayedArg>)
        static_assert(!std::is_lvalue_reference_v<Arg>, "Reference to unique_ptr is not allowed");
}

} // namespace remote_call::impl