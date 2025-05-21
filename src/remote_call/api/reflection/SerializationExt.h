#pragma once

#include "Serialization.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/conversion/detail/Detail.h"
#include "ll/api/Expected.h"

class CompoundTag;

// Extend Serialization by adl
namespace remote_call {
using ll::Expected;

namespace reflection {
template <typename T, typename J>
concept SerializableTo = requires(J& j, T&& val) { serialize_to(j, std::forward<T>(val)); };
template <typename J, typename T>
concept DeserializableFrom = requires(J& j, T& val) { deserialize_to(j, std::forward<T>(val)); };

} // namespace reflection

// remote_call::DynamicValue -> T
template <concepts::SupportToValue T, typename J>
    requires(concepts::IsDynamicValue<std::decay_t<J>>)
inline Expected<> serializeImpl(J& j, T&& val, ll::meta::PriorityTag<6>) {
    return remote_call::detail::toValueInternal(j, std::forward<T>(val));
}

// T -> remote_call::DynamicValue
template <concepts::SupportFromValue T, concepts::IsDynamicValue J>
inline Expected<> deserializeImpl(J& j, T& val, ll::meta::PriorityTag<6>) {
    return detail::fromValueInternal(j, val);
}

// Convert remote_call::DynamicValue to other Universal Type
// Values: variant element of remote_call::Value, such as BlockType
template <concepts::IsVariant T, typename J>
inline Expected<> serializeImpl(J& j, T&& val, ll::meta::PriorityTag<6>) {
    std::visit([&](auto&& val) { reflection::serialize_to(j, std::forward<T>(val)); }, std::forward<T>(val));
}

#if false

template <typename T, typename J>
    requires(
        std::same_as<std::remove_cvref_t<T>, NbtType>
        && (reflection::SerializableTo<CompoundTag*, T> || reflection::SerializableTo<std::unique_ptr<CompoundTag>, T>)
    )
inline Expected<> serializeImpl(J& j, T&& val, ll::meta::PriorityTag<1>) {
    if constexpr (reflection::SerializableTo<class CompoundTag*, T>
                  && reflection::SerializableTo<std::unique_ptr<CompoundTag>, T>) {
        if (val.own) {
            return reflection::serialize_to(j, std::forward<decltype(val.getUniquePtr())>(val.getUniquePtr()));
        } else {
            return reflection::serialize_to(j, std::forward<decltype(val.ptr)>(val.ptr));
        }
    } else if constexpr (reflection::SerializableTo<std::unique_ptr<CompoundTag>, T>) {
        if (val.own) {
            return reflection::serialize_to(j, std::forward<decltype(val.getUniquePtr())>(val.getUniquePtr()));
        } else {
            return reflection::serialize_to(j, std::forward<decltype(val.ptr.clone())>(val.ptr.clone()));
        }
    } else if constexpr (reflection::SerializableTo<CompoundTag*, T>) {
        return reflection::serialize_to(j, std::forward<decltype(val.ptr)>(val.ptr));
    }
}

#endif

} // namespace remote_call