#pragma once

#include "ll/api/base/Containers.h" // IWYU pragma: keep

#include <variant>

// #define LRC_NEW_STRUCT

class Player;
class Actor;
class BlockActor;
class Container;

namespace remote_call {

struct DynamicValue;
struct NumberType;
struct WorldPosType;
struct BlockPosType;
struct ItemType;
struct BlockType;
struct NbtType;

namespace detail {

template <typename T>
using DynamicArrayBase = std::vector<T>;
template <typename T>
#ifdef LRC_NEW_STRUCT
using DynamicObjectBase = ll::SmallStringMap<T>;
#else
using DynamicObjectBase = std::unordered_map<std::string, T>;
#endif
template <typename T, typename... Ty>
using DynamicVariantBase = std::variant<Ty..., DynamicArrayBase<T>, DynamicObjectBase<T>>;

// For ADL
template <typename T>
class DynamicBase : public T {};

#ifdef LRC_NEW_STRUCT

#define AllElementTypes                                                                                                \
    std::monostate, bool, std::string, NumberType, Player*, Actor*, BlockActor*, Container*, WorldPosType,             \
        BlockPosType, ItemType, BlockType, NbtType

using NullType = std::tuple_element_t<0, std::tuple<AllElementTypes>>;

struct DynamicValue : public DynamicBase<DynamicVariantBase<DynamicValue, AllElementTypes>> {
    constexpr DynamicValue()                               = default;
    constexpr DynamicValue(const DynamicValue&)            = delete;
    constexpr DynamicValue(DynamicValue&&)                 = default;
    constexpr DynamicValue& operator=(const DynamicValue&) = delete;
    constexpr DynamicValue& operator=(DynamicValue&&)      = default;
    // clang-format off
    template <typename T> [[nodiscard]] constexpr bool hold() const { return std::holds_alternative<T>(*this); }
    template <typename T> [[nodiscard]] constexpr decltype(auto) get() const& { return std::get<T>(*this); }
    template <typename T> [[nodiscard]] constexpr decltype(auto) get() const&& { return std::get<T>(std::move(*this)); }
    template <typename T> [[nodiscard]] constexpr decltype(auto) get() & { return std::get<T>(*this); }
    template <typename T> [[nodiscard]] constexpr decltype(auto) get() && { return std::get<T>(std::move(*this)); }
    // clang-format on
};

#else

using NullType       = std::nullptr_t;
#define AllElementTypes                                                                                                \
    bool, std::string, NullType, NumberType, Player*, Actor*, BlockActor*, Container*, WorldPosType, BlockPosType,     \
        ItemType, BlockType, NbtType
using DynamicElement = std::variant<AllElementTypes>;

#endif

inline constexpr NullType NULL_VALUE{};

} // namespace detail

} // namespace remote_call
