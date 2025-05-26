#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
class Player;
class Actor;
class BlockActor;
class Container;
using NullType = std::nullptr_t;

namespace remote_call {

struct DynamicValue;
struct NumberType;
struct WorldPosType;
struct BlockPosType;
struct ItemType;
struct BlockType;
struct NbtType;

namespace detail {

/// TODO: std::nullptr_t -> std::monostate, move to first type
#if false
#define AllElementTypes                                                                                                \
    std::monostate, bool, std::string, NumberType, Player*, Actor*, BlockActor*, Container*, WorldPosType,             \
        BlockPosType, ItemType, BlockType, NbtType
using NullType = std::monostate;
#else
#define ExtraType                                                                                                      \
    std::nullptr_t, NumberType, Player*, Actor*, BlockActor*, Container*, WorldPosType, BlockPosType, ItemType,        \
        BlockType, NbtType
#define AllElementTypes bool, std::string, ExtraType
using NullType = std::nullptr_t;
#endif
constexpr NullType NULL_VALUE = NullType{}; // NOLINT: -Wunused-const-variable

using ElementType = std::variant<AllElementTypes>;
template <typename T>
using ArrayTypeBase = std::vector<T>;
template <typename T>
using ObjectTypeBase = std::unordered_map<std::string, T>;
template <typename T>
using VariantTypeBase = std::variant<ElementType, ArrayTypeBase<T>, ObjectTypeBase<T>>;
// For Adl
template <typename T>
class DynamicBase : public VariantTypeBase<T> {};

} // namespace detail
} // namespace remote_call