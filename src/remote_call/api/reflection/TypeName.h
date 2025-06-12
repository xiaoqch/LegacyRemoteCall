#pragma once

#include "ll/api/base/FixedString.h"
#include "ll/api/reflection/TypeName.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call::reflection {

template <typename T>
[[nodiscard]] consteval std::string_view typeName() {
    return ll::reflection::type_raw_name_v<T>;
}
// clang-format off
template <> consteval std::string_view typeName<bool>() { return "bool"; }
template <> consteval std::string_view typeName<std::string>() { return "std::string"; }
template <> consteval std::string_view typeName<NullType>() { return "NullType"; }
template <> consteval std::string_view typeName<NumberType>() { return "NumberType"; }
template <> consteval std::string_view typeName<Player*>() { return "Player*"; }
template <> consteval std::string_view typeName<Actor*>() { return "Actor*"; }
template <> consteval std::string_view typeName<BlockActor*>() { return "BlockActor*"; }
template <> consteval std::string_view typeName<Container*>() { return "Container*"; }
template <> consteval std::string_view typeName<WorldPosType>() { return "WorldPosType"; }
template <> consteval std::string_view typeName<BlockPosType>() { return "BlockPosType"; }
template <> consteval std::string_view typeName<ItemType>() { return "ItemType"; }
template <> consteval std::string_view typeName<BlockType>() { return "BlockType"; }
template <> consteval std::string_view typeName<NbtType>() { return "NbtType"; }
template <> consteval std::string_view typeName<ObjectType>() { return "ObjectType"; }
template <> consteval std::string_view typeName<ArrayType>() { return "ArrayType"; }
template <> consteval std::string_view typeName<VariantType>() { return "VariantType"; }
template <> consteval std::string_view typeName<DynamicValue>() { return "DynamicValue"; }
// clang-format on

template <ll::FixedString sep, typename... T, size_t... N>
    requires(sizeof...(T) == sizeof...(N))
[[nodiscard]] consteval auto typeListNameImpl(std::index_sequence<N...> = std::index_sequence_for<T...>{}) {
    if constexpr (sizeof...(T) == 0) {
        return ll::FixedString{"void"};
    } else {
        constexpr auto toFixedString = []<typename Ty, size_t I>() {
            if constexpr (I == sizeof...(T) - 1) {
                return ll::FixedString<typeName<Ty>().size()>{typeName<Ty>()};
            } else {
                return ll::FixedString<typeName<Ty>().size()>{typeName<Ty>()} + sep;
            }
        };
        return (toFixedString.template operator()<T, N>() + ...);
    }
}

template <ll::FixedString sep, typename... T>
constexpr auto type_list_name_impl_v = typeListNameImpl<sep, T...>(std::index_sequence_for<T...>{});

template <ll::FixedString sep = "|", typename... T>
[[nodiscard]] consteval auto typeListName() {
    static_assert(type_list_name_impl_v<sep, T...>.sv().size() > 0);
    return type_list_name_impl_v<sep, T...>.sv();
}

template <typename... T>
    requires(sizeof...(T) > 0)
[[nodiscard]] consteval auto typeListName() {
    static_assert(type_list_name_impl_v<"|", T...>.sv().size() > 0);
    return type_list_name_impl_v<"|", T...>.sv();
}

static_assert(typeListName<bool>() == "bool");
static_assert(typeListName<>() == "void");
static_assert(typeListName<bool, std::string>() == "bool|std::string");


[[nodiscard]] inline std::string_view typeName(DynamicValue const& dv) {
    constexpr auto getTypeName = [](auto&& v) { return typeName<std::decay_t<decltype(v)>>(); };
    if (dv.hold<ElementType>()) {
        return std::visit(getTypeName, dv.get<ElementType>());
    } else {
        return std::visit(getTypeName, dv);
    }
}

} // namespace remote_call::reflection