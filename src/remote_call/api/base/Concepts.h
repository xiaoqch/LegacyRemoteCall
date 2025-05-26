#pragma once

#include "ll/api/base/Concepts.h"
#include "ll/api/base/Meta.h"
#include "mc/world/level/BlockPos.h"
#include "remote_call/api/value/Base.h"

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

namespace concepts {

using ll::concepts::IsLeviExpected;
using ll::meta::PriorityTag;

template <typename T>
concept IsDynamicValue = std::same_as<T, DynamicValue>;

template <typename T>
concept IsNormalElement =
    ll::concepts::IsOneOf<std::decay_t<T>, bool, std::string, NullType, Player*, Actor*, BlockActor*, Container*>;
template <typename T>
concept IsCustomElement =
    ll::concepts::IsOneOf<std::decay_t<T>, NumberType, WorldPosType, BlockPosType, ItemType, BlockType, NbtType>;
template <typename T>
concept IsValueElement = IsNormalElement<T> || IsCustomElement<T>;

template <typename T, typename V>
concept CompatibleWithCustomElement_ = IsCustomElement<V> && requires(V n, T&& t) {
    n.template get<std::decay_t<T>>();
    V(std::forward<T>(t));
};

template <typename T>
concept IsUniquePtr = requires {
    typename T::element_type;
    typename T::deleter_type;
} && std::is_same_v<T, std::unique_ptr<typename T::element_type, typename T::deleter_type>>;

template <typename T, typename V>
concept CompatibleWithCustomElement =
    CompatibleWithCustomElement_<T, V>
    || (IsUniquePtr<std::decay_t<T>> && CompatibleWithCustomElement_<typename std::decay_t<T>::pointer, V>);

template <typename T>
concept IsString = ll::concepts::IsString<T> && !std::same_as<std::decay_t<T>, nullptr_t>;

template <typename T>
concept IsPos =
    std::same_as<std::remove_cvref_t<T>, class BlockPos> || std::same_as<std::remove_cvref_t<T>, class Vec3>;

template <typename T>
concept IsVariant = requires(T t) {
    std::visit([](auto&&) {}, std::forward<T>(t));
    std::variant_size_v<T>;
    std::variant_alternative<0, T>::type;
};

template <typename T>
concept IsTupleLike = requires(T&& t) {
    std::tuple_size<std::decay_t<T>>::value;
    []<std::size_t... I>(T&& t, std::index_sequence<I...>) {
        ((void)std::get<I>(std::forward<T>(t)), ...);
    }(std::forward<T>(t), std::make_index_sequence<std::tuple_size<std::decay_t<T>>::value>{});
};

static_assert(IsTupleLike<std::pair<int, int>&>);
static_assert(IsTupleLike<std::tuple<std::unique_ptr<int>>>);
static_assert(IsTupleLike<std::tuple<>&&>);
static_assert(!IsTupleLike<int>);
static_assert(!IsTupleLike<void>);

} // namespace concepts
} // namespace remote_call