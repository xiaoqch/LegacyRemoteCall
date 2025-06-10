#pragma once

#include "ll/api/base/Containers.h"


namespace remote_call {
struct TransparentPairHash {
    using is_transparent = void;
    size_t operator()(const std::pair<std::string, std::string>& p) const {
        return std::hash<std::string_view>{}(p.first) ^ (std::hash<std::string_view>{}(p.second) << 1);
    }
    size_t operator()(const std::pair<std::string_view, std::string_view>& p) const {
        return std::hash<std::string_view>{}(p.first) ^ (std::hash<std::string_view>{}(p.second) << 1);
    }
};

struct TransparentPairEqual {
    using is_transparent = void;

    bool operator()(const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) const {
        return a.first == b.first && a.second == b.second;
    }
    bool operator()(
        const std::pair<std::string_view, std::string_view>& a,
        const std::pair<std::string, std::string>&           b
    ) const {
        return a.first == b.first && a.second == b.second;
    }
    bool operator()(
        const std::pair<std::string, std::string>&           a,
        const std::pair<std::string_view, std::string_view>& b
    ) const {
        return a.first == b.first && a.second == b.second;
    }
    bool operator()(
        const std::pair<std::string_view, std::string_view>& a,
        const std::pair<std::string_view, std::string_view>& b
    ) const {
        return a.first == b.first && a.second == b.second;
    }
};

template <class Value, class... Rests>
using IdentifierMap =
    ll::DenseMap<std::pair<std::string, std::string>, Value, TransparentPairHash, TransparentPairEqual, Rests...>;

template <typename T>
constexpr bool has_embedded_set_v = requires { typename T::EmbeddedSet; };

[[maybe_unused]] constexpr bool IdentifierMapIsParallelMap = has_embedded_set_v<IdentifierMap<int>>;

} // namespace remote_call