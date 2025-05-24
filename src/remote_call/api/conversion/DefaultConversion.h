#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/Container.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/conversion/detail/Detail.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call {

// bool, std::string, NullType, Player*, Actor*, BlockActor*, Container*
template <concepts::IsNormalElement T>
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::DefaultTag) {
    if constexpr (std::is_pointer_v<std::decay_t<T>>) {
        if (t == nullptr) {
            v.emplace<ElementType>(NullValue);
            return {};
        }
    }
    v.emplace<ElementType>(std::forward<T>(t));
    return {};
}
template <concepts::IsNormalElement T>
inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::DefaultTag) {
    if constexpr (std::is_pointer_v<T>) {
        if (v.hold<NullType>()) {
            t = T{};
            return {};
        }
    }
    if (v.hold<T>()) t = v.get<T>();
    else return error_utils::makeFromDynamicTypeError<T, T>(v);
    return {};
}

// String like
template <concepts::IsString T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::LowTag) {
    v.emplace<ElementType>(std::string{std::forward<T>(t)});
    return {};
}
template <concepts::IsString T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::LowTag) {
    if (v.hold<std::string>()) t = std::forward<std::string>(v.get<std::string>());
    else return error_utils::makeFromDynamicTypeError<T, std::string>(v);
    return {};
}

namespace detail {

template <typename T, concepts::IsCustomElement V>
    requires(concepts::CompatibleWithCustomElement<T, V>)
struct CustomElementConverter {
public:
    static inline ll::Expected<> toDynamic(DynamicValue& v, T&& t)
        requires(!IsUniquePtr<std::remove_reference_t<T>>)
    {
        v.emplace<ElementType>(V(std::forward<T>(t)));
        return {};
    };
    static inline ll::Expected<> toDynamic(DynamicValue& v, T& t)
        requires(IsUniquePtr<std::remove_reference_t<T>>)
    {
        v.emplace<ElementType>(V(std::forward<T>(t).get()));
        return {};
    };
    static inline ll::Expected<> fromDynamic(DynamicValue& v, T& t) {
        if (v.hold<V>()) t = static_cast<T>(std::forward<T>(v.get<V>().template get<T>()));
        else return error_utils::makeFromDynamicTypeError<T, V>(v);
        return {};
    };
};

#define COMPACT_TYPE_CONVERTER(TYPE)                                                                                   \
    template <concepts::CompatibleWithCustomElement<TYPE> T>                                                           \
        requires(!ll::concepts::IsOneOf<T, AllElementTypes>)                                                           \
    inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::LowTag) {                                          \
        return detail::CustomElementConverter<T, TYPE>::toDynamic(v, std::forward<T>(t));                                \
    }                                                                                                                  \
    template <concepts::CompatibleWithCustomElement<TYPE> T>                                                           \
        requires(!ll::concepts::IsOneOf<T, AllElementTypes>)                                                           \
    inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::LowTag) {                                         \
        return detail::CustomElementConverter<T, TYPE>::fromDynamic(v, t);                                               \
    }

} // namespace detail

COMPACT_TYPE_CONVERTER(NbtType);
COMPACT_TYPE_CONVERTER(BlockType);
COMPACT_TYPE_CONVERTER(ItemType);
COMPACT_TYPE_CONVERTER(NumberType);
// COMPACT_TYPE_CONVERTER(BlockPosType);
// COMPACT_TYPE_CONVERTER(WorldPosType);

// Actor*, Player*...
template <std::convertible_to<Actor const*> T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::LowTag) {
    if constexpr (!std::convertible_to<T, Player const*>)
        v.template emplace<ElementType>(const_cast<Actor*>(static_cast<Actor const*>(std::forward<decltype(t)>(t))));
    else v.template emplace<ElementType>(const_cast<Player*>(static_cast<Player const*>(std::forward<decltype(t)>(t))));
    return {};
}
template <std::convertible_to<Actor const*> T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::LowTag) {
    if constexpr (std::convertible_to<T, Player const*>) {
        if (v.hold<Player*>()) {
            auto ptr = v.get<Player*>();
            if constexpr (std::convertible_to<T, SimulatedPlayer const*>) {
                if (ptr && !ptr->isSimulatedPlayer()) return ll::makeStringError("Player is not SimulatedPlayer");
            }
            t = reinterpret_cast<T>(ptr);
            return {};
        } else {
            return error_utils::makeFromDynamicTypeError<T, Player*>(v);
        }
    } else if constexpr (std::convertible_to<Actor*, T>) {
        if (v.hold<Actor*>()) t = v.get<Actor*>();
        else return error_utils::makeFromDynamicTypeError<T, Actor*>(v);
        return {};
    } else {
        static_assert(false, "Unsafety convert from value to Entity");
    }
}

// BlockPod, Vec3
template <concepts::IsPos T>
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::DefaultTag) {
    v.emplace<ElementType>(std::forward<T>(t));
    return {};
}
template <concepts::IsPos T>
inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::DefaultTag) {
    if (v.hold<BlockPosType>()) {
        auto& val = v.get<BlockPosType>();
        t         = val.pos;
        return {};
    } else if (v.hold<WorldPosType>()) {
        auto& val = v.get<WorldPosType>();
        t         = val.pos;
        return {};
    } else {
        return error_utils::makeFromDynamicTypeError<T, BlockPosType, WorldPosType>(v);
    }
}

// std::tuple<BlockPos,int>, std::pair<Vec3,int>
template <concepts::IsTupleLike Pos>
    requires(std::tuple_size<std::decay_t<Pos>>::value == 2 &&requires(Pos pos) {
        { std::get<0>(pos) } -> concepts::IsPos;
        { std::get<1>(pos) } -> std::convertible_to<int>;
    } )
inline ll::Expected<> toDynamic(DynamicValue& v, Pos&& t, priority::LowTag) {
    using PosType = decltype(std::get<0>(std::forward<Pos>(t)));
    v.emplace<ElementType>(std::pair<std::decay_t<PosType>, int>{
        std::get<0>(std::forward<Pos>(t)),
        static_cast<int>(std::get<1>(std::forward<Pos>(t)))
    });
    return {};
}
template <typename Pos>
    requires(std::tuple_size<std::decay_t<Pos>>::value == 2 && requires(Pos pos) {
        { std::get<0>(pos) } -> concepts::IsPos;
        { std::get<1>(pos) } -> std::convertible_to<int>;
    } )
inline ll::Expected<> fromDynamic(DynamicValue& v, Pos& t, priority::LowTag) {
    using DimType = decltype(std::get<1>(std::forward<Pos>(t)));
    if (v.hold<BlockPosType>()) {
        auto& val      = v.get<BlockPosType>();
        std::get<0>(t) = val.pos;
        std::get<1>(t) = static_cast<DimType>(val.dimId);
        return {};
    }
    if (v.hold<WorldPosType>()) {
        auto& val      = v.get<WorldPosType>();
        std::get<0>(t) = val.pos;
        std::get<1>(t) = static_cast<DimType>(val.dimId);
        return {};
    }
    return error_utils::makeFromDynamicTypeError<Pos, BlockPosType, WorldPosType>(v);
}

// T& -> T* -> remote_call::DynamicValue
// T: Player, BlockActor...
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportToDynamic<std::add_pointer_t<std::remove_reference_t<T>>>)
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::LowTag) {
    return detail::toDynamicInternal(v, std::forward<std::add_pointer_t<std::remove_reference_t<T>>>(&t));
}
// template <typename T>
//     requires(std::is_lvalue_reference_v<T> &&
//     concepts::SupportFromDynamic<std::add_pointer_t<std::remove_reference_t<T>>>)
// inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::DefaultTag) {
//     return detail::fromDynamicInternal(v, std::forward<std::add_pointer_t<std::remove_reference_t<T>>>(&t));
// }

// std::optional<T>
template <ll::concepts::IsOptional T>
    requires(concepts::SupportFromDynamic<typename T::value_type>)
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::LowTag) {
    if (t.has_value()) return detail::toDynamicInternal(v, *std::forward<T>(t));
    else v.emplace<ElementType>(NullValue);
    return {};
}
template <ll::concepts::IsOptional T>
    requires(concepts::SupportToDynamic<typename T::value_type>)
inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::LowTag) {
    if (v.hold<NullType>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = detail::fromDynamicInternal(v, val);
        if (res) t = val;
        return res;
    }
}

#if false

namespace concepts {
template <typename T>
concept IsPointer = std::is_pointer_v<T>;

template <typename T>
concept IsOptionalRef = requires(std::decay_t<T> v) {
    { v.as_ptr() } -> IsPointer;
    v.has_value();
    v = v.as_ptr();
};
static_assert(IsOptionalRef<optional_ref<Actor>>);
} // namespace concepts

// Error
// optional_ref;
template <concepts::IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::LowTag) {
    if (t.has_value()) return detail::toDynamicInternal(v, std::forward<T>(t).as_ptr());
    else v.emplace<ElementType>(NullValue);
    return {};
}
template <concepts::IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
inline ll::Expected<> fromDynamic(DynamicValue& v, T& t, priority::DefaultTag) {
    if (v.hold<NullType>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = detail::fromDynamicInternal(v, val);
        if (res) t = val;
        return res;
    }
}
#endif

template <typename T>
    requires(std::same_as<std::decay_t<T>, Block*>)
inline ll::Expected<> toDynamic(DynamicValue& v, T&& t, priority::PriorityTag<1>) {
    // static_assert(false, "'Block*' is unsupported type, use 'Block const*' instead.");
    v.emplace<ElementType>(const_cast<Block*>(t));
    return {};
}

} // namespace remote_call
