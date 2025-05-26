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
#include "remote_call/api/conversions/detail/Detail.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/Base.h"
#include "remote_call/api/value/DynamicValue.h"

namespace remote_call::detail {

// bool, std::string, NullType, Player*, Actor*, BlockActor*, Container*
template <concepts::IsNormalElement T>
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::HightTag) {
    if constexpr (std::is_pointer_v<std::decay_t<T>>) {
        if (t == nullptr) {
            dv.emplace<ElementType>(NULL_VALUE);
            return {};
        }
    }
    dv.emplace<ElementType>(std::forward<T>(t));
    return {};
}
template <concepts::IsNormalElement T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::HightTag) {
    if constexpr (std::is_pointer_v<T>) {
        if (dv.hold<NullType>()) {
            t = T{};
            return {};
        }
    }
    if (dv.hold<T>()) t = dv.get<T>();
    else return error_utils::makeFromDynamicTypeError<T, T>(dv);
    return {};
}

// String like
template <concepts::IsString T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::LowTag) {
    dv.emplace<ElementType>(std::string{std::forward<T>(t)});
    return {};
}
template <concepts::IsString T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::LowTag) {
    if (dv.hold<std::string>()) t = std::forward<DynamicValue>(dv).get<std::string>();
    else return error_utils::makeFromDynamicTypeError<T, std::string>(dv);
    return {};
}

namespace detail {

template <typename NativeType, concepts::IsCustomElement V>
    requires(concepts::CompatibleWithCustomElement<NativeType, V>)
struct CustomElementConverter {
public:
    template <typename T = NativeType>
    static inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t)
        requires(!IsUniquePtr<std::decay_t<T>>)
    {
        dv.emplace<ElementType>(V(std::forward<T>(t)));
        return {};
    };
    template <typename T = NativeType>
    static inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t)
        requires(IsUniquePtr<std::decay_t<T>>)
    {
        dv.emplace<ElementType>(V(std::forward<T>(t)));
        return {};
    };
    template <typename T = NativeType>
    static inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t) {
        if (dv.hold<V>()) t = static_cast<T>(std::forward<T>(dv.get<V>().template get<T>()));
        else return error_utils::makeFromDynamicTypeError<T, V>(dv);
        return {};
    };
};

#define COMPACT_TYPE_CONVERTER(TYPE)                                                                                   \
    template <concepts::CompatibleWithCustomElement<TYPE> T>                                                           \
        requires(!ll::concepts::IsOneOf<T, AllElementTypes>)                                                           \
    inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::DefaultTag) {                                   \
        return detail::CustomElementConverter<std::decay_t<T>, TYPE>::toDynamic(dv, std::forward<T>(t));               \
    }                                                                                                                  \
    template <concepts::CompatibleWithCustomElement<TYPE> T>                                                           \
        requires(!ll::concepts::IsOneOf<T, AllElementTypes>)                                                           \
    inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::DefaultTag) {                                  \
        return detail::CustomElementConverter<std::decay_t<T>, TYPE>::fromDynamic(dv, t);                              \
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
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::DefaultTag) {
    if constexpr (!std::convertible_to<T, Player const*>)
        dv.template emplace<ElementType>(const_cast<Actor*>(static_cast<Actor const*>(std::forward<decltype(t)>(t))));
    else
        dv.template emplace<ElementType>(const_cast<Player*>(static_cast<Player const*>(std::forward<decltype(t)>(t))));
    return {};
}

template <std::convertible_to<Actor const*> T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::DefaultTag) {
    if constexpr (std::convertible_to<T, Player const*>) {
        if (dv.hold<Player*>()) {
            auto ptr = dv.get<Player*>();
            if constexpr (std::convertible_to<T, SimulatedPlayer const*>) {
                if (ptr && !ptr->isSimulatedPlayer()) return ll::makeStringError("Player is not SimulatedPlayer");
            }
            t = reinterpret_cast<T>(ptr);
            return {};
        } else {
            return error_utils::makeFromDynamicTypeError<T, Player*>(dv);
        }
    } else if constexpr (std::convertible_to<Actor*, T>) {
        if (dv.hold<Actor*>()) t = dv.get<Actor*>();
        else return error_utils::makeFromDynamicTypeError<T, Actor*>(dv);
        return {};
    } else {
        static_assert(false, "Unsafety convert from value to Entity");
    }
}

// BlockPod, Vec3
template <concepts::IsPos T>
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::HightTag) {
    dv.emplace<ElementType>(std::forward<T>(t));
    return {};
}
template <concepts::IsPos T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::HightTag) {
    if (dv.hold<BlockPosType>()) {
        auto&& val = std::move(dv).get<BlockPosType>();
        t          = val.pos;
        return {};
    } else if (dv.hold<WorldPosType>()) {
        auto& val = dv.get<WorldPosType>();
        t         = val.pos;
        return {};
    } else {
        return error_utils::makeFromDynamicTypeError<T, BlockPosType, WorldPosType>(dv);
    }
}

// std::tuple<BlockPos,int>, std::pair<Vec3,int>
template <concepts::IsTupleLike Pos>
    requires(std::tuple_size<std::decay_t<Pos>>::value == 2 &&requires(Pos pos) {
        { std::get<0>(pos) } -> concepts::IsPos;
        { std::get<1>(pos) } -> std::convertible_to<int>;
    } )
inline ll::Expected<> toDynamic(DynamicValue& dv, Pos&& t, priority::DefaultTag) {
    using PosType = decltype(std::get<0>(std::forward<Pos>(t)));
    dv.emplace<ElementType>(std::pair<std::decay_t<PosType>, int>{
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
inline ll::Expected<> fromDynamic(DynamicValue& dv, Pos& t, priority::DefaultTag) {
    using DimType = decltype(std::get<1>(std::forward<Pos>(t)));
    if (dv.hold<BlockPosType>()) {
        auto& val      = dv.get<BlockPosType>();
        std::get<0>(t) = val.pos;
        std::get<1>(t) = static_cast<DimType>(val.dimId);
        return {};
    }
    if (dv.hold<WorldPosType>()) {
        auto& val      = dv.get<WorldPosType>();
        std::get<0>(t) = val.pos;
        std::get<1>(t) = static_cast<DimType>(val.dimId);
        return {};
    }
    return error_utils::makeFromDynamicTypeError<Pos, BlockPosType, WorldPosType>(dv);
}

// T& -> T* -> remote_call::DynamicValue
// T: Player, BlockActor...
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportToDynamic<std::add_pointer_t<std::remove_reference_t<T>>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::LowTag) {
    return ::remote_call::toDynamic(dv, std::forward<std::add_pointer_t<std::remove_reference_t<T>>>(&t));
}

// template <typename T>
//     requires(std::is_lvalue_reference_v<T> &&
//     concepts::SupportFromDynamic<std::add_pointer_t<std::remove_reference_t<T>>>)
// inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::HightTag) {
//     return ::remote_call::fromDynamic(dv, std::forward<std::add_pointer_t<std::remove_reference_t<T>>>(&t));
// }

template <typename T>
    requires(std::same_as<std::decay_t<T>, Block*>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::PriorityTag<1>) {
    // static_assert(false, "'Block*' is unsupported type, use 'Block const*' instead.");
    dv.emplace<ElementType>(const_cast<Block*>(t));
    return {};
}

} // namespace remote_call::detail
