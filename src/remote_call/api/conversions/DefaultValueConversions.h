#pragma once

#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "magic_enum.hpp"
#include "magic_enum_flags.hpp"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/Container.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/base/TypeTraits.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call::detail {

// bool, std::string, NullType, Player*, Actor*, BlockActor*, Container*
template <typename T>
    requires(concepts::IsNormalElement<std::decay_t<T>>)
inline ll::Expected<>
toDynamic(DynamicValue& dv, T&& t, priority::HightTag) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
    if constexpr (std::is_pointer_v<std::decay_t<T>>) {
        if (t == nullptr) {
            dv.emplace<NullType>(NULL_VALUE);
            return {};
        }
    }
    dv.emplace<std::decay_t<T>>(std::forward<T>(t));
    return {};
}
template <concepts::IsNormalElement T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::HightTag) {
    if (dv.hold<T>()) [[likely]] {
        t = dv.get<T>();
        return {};
    }
    if constexpr (std::is_pointer_v<T>) {
        if (dv.hold<NullType>()) [[likely]] {
            t = T{};
            return {};
        }
    }
    return error_utils::makeFromDynamicTypeError<T, T>(dv);
}

// String like
template <concepts::IsString T>
    requires(!concepts::IsValueElement<std::decay_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::LowTag) noexcept {
    dv.emplace<std::string>(std::forward<T>(t));
    return {};
}
template <concepts::IsString T>
    requires(!concepts::IsValueElement<T>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::LowTag) {
    if (dv.hold<std::string>()) [[likely]] {
        t = std::forward<DynamicValue>(dv).get<std::string>();
        return {};
    } else {
        return error_utils::makeFromDynamicTypeError<T, std::string>(dv);
    }
}

// NbtType, BlockType, ItemType, NumberType
template <typename NativeType, typename DynType>
    requires(concepts::IsCustomElement<std::decay_t<DynType>> && concepts::CompatibleWithCustomElement<NativeType, DynType>)
struct CustomElementConverter {
public:
    template <typename T = NativeType>
        requires(!concepts::IsUniquePtr<std::decay_t<T>>)
    inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t) const
        noexcept(noexcept(dv.emplace<DynType>(std::forward<T>(t)))) {
        dv.emplace<DynType>(std::forward<T>(t));
        return {};
    };
    template <typename T = NativeType>
        requires(concepts::IsUniquePtr<std::decay_t<T>>)
    inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t) const
        noexcept(noexcept(dv.emplace<DynType>(std::forward<T>(t)))) {
        dv.emplace<DynType>(std::forward<T>(t));
        return {};
    };
    template <typename T = NativeType>
    inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t) const {
        if (!dv.hold<DynType>()) [[unlikely]] {
            return error_utils::makeFromDynamicTypeError<T, DynType>(dv);
        }
        t = dv.get<DynType>().template get<T>();
        return {};
    };
};

template <typename NativeType, typename DynType>
inline constexpr auto custom_element_converter_v = static_const<CustomElementConverter<NativeType, DynType>>::value;

#define COMPATIBLE_TYPE_CONVERTER(TYPE)                                                                                \
    template <concepts::CompatibleWithCustomElement<TYPE> T>                                                           \
        requires(!ll::concepts::IsOneOf<T, AllElementTypes>)                                                           \
    inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::DefaultTag) noexcept(                           \
        noexcept(custom_element_converter_v<std::decay_t<T>, TYPE>.toDynamic(dv, std::forward<T>(t)))                  \
    ) {                                                                                                                \
        return custom_element_converter_v<std::decay_t<T>, TYPE>.toDynamic(dv, std::forward<T>(t));                    \
    }                                                                                                                  \
    template <concepts::CompatibleWithCustomElement<TYPE> T>                                                           \
        requires(!ll::concepts::IsOneOf<T, AllElementTypes>)                                                           \
    inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::DefaultTag) {                                  \
        return custom_element_converter_v<std::decay_t<T>, TYPE>.fromDynamic(dv, t);                                   \
    }

COMPATIBLE_TYPE_CONVERTER(NbtType);
COMPATIBLE_TYPE_CONVERTER(BlockType);
COMPATIBLE_TYPE_CONVERTER(ItemType);
COMPATIBLE_TYPE_CONVERTER(NumberType);
// COMPATIBLE_TYPE_CONVERTER(BlockPosType);
// COMPATIBLE_TYPE_CONVERTER(WorldPosType);

// Actor*, Player*...
template <std::convertible_to<Actor const*> T>
    requires(!concepts::IsValueElement<std::decay_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::DefaultTag) noexcept {
    using PtrType = std::conditional_t<std::convertible_to<T, Player const*>, Player, Actor>;
    dv.template emplace<PtrType*>(const_cast<PtrType*>(static_cast<PtrType const*>(std::forward<decltype(t)>(t))));
    return {};
}

// Player*, ServerPlayer*
template <std::convertible_to<Player const*> T>
    requires(!concepts::IsValueElement<T> && !std::convertible_to<T, SimulatedPlayer const*>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::DefaultTag) {
    if (!dv.hold<Player*>()) [[unlikely]] {
        return error_utils::makeFromDynamicTypeError<T, Player*>(dv);
    }
    t = reinterpret_cast<T>(dv.get<Player*>());
    return {};
}

// Actor*
template <typename T>
    requires(!concepts::IsValueElement<T> && std::convertible_to<Actor*, T>)
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::DefaultTag) {
    if (dv.hold<Player*>()) {
        t = dv.get<Player*>();
        return {};
    } else if (dv.hold<Actor*>()) {
        t = dv.get<Actor*>();
        return {};
    } else [[unlikely]] {
        return error_utils::makeFromDynamicTypeError<T, Actor*, Player*>(dv);
    }
}

// BlockPod, Vec3
template <concepts::IsPos T>
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::HightTag) noexcept {
    using DynPosType = std::conditional_t<std::same_as<std::remove_cvref_t<T>, BlockPos>, BlockPosType, WorldPosType>;
    dv.emplace<DynPosType>(std::forward<T>(t));
    return {};
}

template <concepts::IsPos T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t, priority::HightTag) {
    if (dv.hold<BlockPosType>()) {
        t = dv.get<BlockPosType>().get<T>();
        return {};
    } else if (dv.hold<WorldPosType>()) {
        t = dv.get<WorldPosType>().get<T>();
        return {};
    } else [[unlikely]] {
        return error_utils::makeFromDynamicTypeError<T, BlockPosType, WorldPosType>(dv);
    }
}

// std::tuple<BlockPos,int>, std::pair<Vec3,int>
template <concepts::IsTupleLike Pos>
    requires(std::tuple_size<std::decay_t<Pos>>::value == 2 &&requires(Pos pos) {
        { std::get<0>(pos) } -> concepts::IsPos;
        { std::get<1>(pos) } -> std::convertible_to<int>;
    } )
inline ll::Expected<> toDynamic(DynamicValue& dv, Pos&& t, priority::DefaultTag) noexcept {
    using PosType    = std::decay_t<decltype(std::get<0>(std::forward<Pos>(t)))>;
    using DynPosType = std::conditional_t<std::same_as<PosType, BlockPos>, BlockPosType, WorldPosType>;
    dv.emplace<DynPosType>(std::get<0>(std::forward<Pos>(t)), static_cast<int>(std::get<1>(std::forward<Pos>(t))));
    return {};
}
template <typename Pos>
    requires(std::tuple_size<std::decay_t<Pos>>::value == 2 && requires(Pos pos) {
        { std::get<0>(pos) } -> concepts::IsPos;
        { std::get<1>(pos) } -> std::convertible_to<int>;
    } )
inline ll::Expected<> fromDynamic(DynamicValue& dv, Pos& t, priority::DefaultTag) {
    using PosType = std::decay_t<decltype(std::get<0>(std::forward<Pos>(t)))>;
    using DimType = std::decay_t<decltype(std::get<1>(std::forward<Pos>(t)))>;
    if (dv.hold<BlockPosType>()) {
        auto& val      = dv.get<BlockPosType>();
        std::get<0>(t) = val.get<PosType>();
        std::get<1>(t) = static_cast<DimType>(val.dimId);
        return {};
    } else if (dv.hold<WorldPosType>()) {
        auto& val      = dv.get<WorldPosType>();
        std::get<0>(t) = val.get<PosType>();
        std::get<1>(t) = static_cast<DimType>(val.dimId);
        return {};
    } else [[unlikely]] {
        return error_utils::makeFromDynamicTypeError<Pos, BlockPosType, WorldPosType>(dv);
    }
}

// T& -> T* -> remote_call::DynamicValue
// T: Player, BlockActor...
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportToDynamic<traits::reference_to_pointer_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::LowTag) noexcept(
    noexcept(::remote_call::toDynamic(dv, std::forward<traits::reference_to_pointer_t<T>>(&t)))
) {
    return ::remote_call::toDynamic(dv, std::forward<traits::reference_to_pointer_t<T>>(&t));
}

// reference_wrapper<T> -> T* -> remote_call::DynamicValue
template <typename T>
    requires(traits::is_reference_wrapper_v<std::decay_t<T>> && concepts::SupportToDynamic<traits::reference_to_pointer_t<std::decay_t<T>>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::LowTag) noexcept(
    noexcept(::remote_call::toDynamic(dv, std::forward<traits::reference_to_pointer_t<std::decay_t<T>>>(&t.get())))
) {
    return ::remote_call::toDynamic(dv, std::forward<traits::reference_to_pointer_t<std::decay_t<T>>>(&t.get()));
}

// remote_call::DynamicValue -> T* -> std::reference_wrapper<T>
template <typename T>
    requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<traits::reference_to_pointer_t<T>>)
LL_CONSTEXPR23 ll::Expected<traits::reference_to_wrapper_t<T>>
               fromDynamic(DynamicValue& dv, std::in_place_type_t<T>, priority::LowTag) {
    return dv.tryGet<traits::reference_to_pointer_t<T>>().transform(
        [](auto&& ptr) -> traits::reference_to_wrapper_t<T> { return std::ref(*ptr); }
    );
}

// remote_call::DynamicValue -> T* -> std::reference_wrapper<T>
template <typename T>
    requires(traits::is_reference_wrapper_v<T> && concepts::SupportFromDynamic<traits::reference_to_pointer_t<T>>)
LL_CONSTEXPR23 ll::Expected<traits::reference_to_wrapper_t<T>>
               fromDynamic(DynamicValue& dv, std::in_place_type_t<T>, priority::LowTag) {
    return dv.tryGet<traits::reference_to_pointer_t<T>>().transform(
        [](auto&& ptr) -> traits::reference_to_wrapper_t<T> { return std::ref(*ptr); }
    );
}

// enum
template <class T>
    requires(std::is_enum_v<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamicEnumName(DynamicValue& dv, T&& e, ll::meta::PriorityTag<1>) {
    using EnumType = std::remove_cvref_t<T>;
    std::string_view name{};
    if constexpr (magic_enum::detail::subtype_v<EnumType> == magic_enum::detail::enum_subtype::flags) {
        name = magic_enum::enum_flags_name<EnumType>(e);
    } else {
        name = magic_enum::enum_name<EnumType>(e);
    }
    if (name.empty()) [[unlikely]] {
        return error_utils::makeUnsupportedValueError<T, std::string>(
            std::to_string(static_cast<std::underlying_type_t<EnumType>>(e)),
            "can not get enum name"
        );
    }
    dv.emplace<std::string>(name);
    return {};
}

template <class T>
    requires(std::is_enum_v<std::remove_cvref_t<T>>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& e, priority::LowTag) noexcept {
    dv.emplace<NumberType>(static_cast<std::underlying_type_t<std::remove_cvref_t<T>>>(e));
    return {};
}

template <ll::concepts::Require<std::is_enum> T>
inline ll::Expected<> fromDynamicEnumName(std::string& dv, T& e) {
    using EnumType = std::remove_cvref_t<T>;
    std::optional<EnumType> res;
    if constexpr (magic_enum::detail::subtype_v<EnumType> == magic_enum::detail::enum_subtype::flags) {
        res = magic_enum::enum_flags_cast<EnumType>(dv);
    } else {
        res = magic_enum::enum_cast<EnumType>(dv);
    }
    if (!res) [[unlikely]]
        return error_utils::makeUnsupportedValueError<std::string, T>(dv, "can not cast to enum");
    e = *res;
    return {};
}

template <ll::concepts::Require<std::is_enum> T>
inline ll::Expected<> fromDynamic(DynamicValue& dv, T& e, priority::LowTag) {
    using EnumType = std::remove_cvref_t<T>;
    if constexpr (magic_enum::detail::is_reflected_v<EnumType, magic_enum::detail::subtype_v<EnumType>>) {
        if (dv.is_string()) {
            return fromDynamicEnumName(dv.get<std::string>(), e);
        }
    }
    if (dv.is_number()) {
        e = static_cast<EnumType>(dv.get<NumberType>().get<std::underlying_type_t<EnumType>>());
    } else [[unlikely]] {
        return error_utils::makeFromDynamicTypeError<T, std::string, NumberType>(dv);
    }
    return {};
}

// Block*
template <typename T>
    requires(std::same_as<std::decay_t<T>, Block*>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&& t, priority::PriorityTag<1>) noexcept {
    // static_assert(false, "'Block*' is unsupported type, use 'Block const*' instead.");
    dv.emplace<BlockType>(const_cast<Block*>(t));
    return {};
}

// std::nullopt_t, std::monostate
template <typename T>
    requires(ll::concepts::IsOneOf<std::decay_t<T>, std::nullopt_t, std::monostate>)
inline ll::Expected<> toDynamic(DynamicValue& dv, T&&, priority::DefaultTag) noexcept {
    dv.emplace<NullType>(NULL_VALUE);
    return {};
}

template <ll::concepts::IsOneOf<std::nullopt_t, std::monostate> T>
inline ll::Expected<> fromDynamic(DynamicValue&, T& t, priority::DefaultTag) noexcept {
    t = {};
    return {};
}

} // namespace remote_call::detail
