#pragma once

#include "Values.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "remote_call/api/base/TypeTraits.h"

namespace remote_call {
// NOLINTBEGIN: google-explicit-constructor

struct DynamicValue;

namespace detail {
template <concepts::SupportToDynamic T>
[[nodiscard]] inline ll::Expected<> toDynamicInternal(DynamicValue& dv, T&& val);
template <concepts::SupportFromDynamic T>
[[nodiscard]] inline ll::Expected<> fromDynamicInternal(DynamicValue& dv, T& t);
} // namespace detail

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
constexpr NullType NullValue = NullType{};

using ElementType = std::variant<AllElementTypes>;
using ArrayType   = std::vector<DynamicValue>;
using ObjectType  = std::unordered_map<std::string, DynamicValue>;
using VariantType = std::variant<ElementType, ArrayType, ObjectType>;

struct DynamicValue : public VariantType {
    DynamicValue() : VariantType(ElementType{NullValue}) {};
    DynamicValue(const DynamicValue&)            = delete;
    DynamicValue(DynamicValue&&)                 = default;
    DynamicValue& operator=(const DynamicValue&) = delete;
    DynamicValue& operator=(DynamicValue&&)      = default;
    template <typename T>
        requires(!std::is_reference_v<T> && traits::is_variant_alternative_v<VariantType, T>)
    explicit DynamicValue(T&& v) : VariantType(std::forward<T>(v)){};
    template <typename T>
        requires(!std::is_reference_v<T> && traits::is_variant_alternative_v<VariantType, T>)
    DynamicValue& operator=(T&& v) {
        this->emplace<T>(std::forward<T>(v));
        return *this;
    }
    template <typename T>
        requires(requires(T&& v) { ElementType(std::forward<T>(v)); })
    DynamicValue(T&& v) : VariantType(ElementType(std::forward<T>(v))){};
    template <concepts::SupportToDynamic T>
        requires(!requires(T&& v) { ElementType(std::forward<T>(v)); } && !std::same_as<T, DynamicValue>)
    DynamicValue(T&& v) : VariantType() {
        ll::Expected<> res = detail::toDynamicInternal(*this, std::forward<T>(v));
        if (!res) {
            *this = {NullValue};
            throw std::runtime_error(res.error().message());
        }
    };
    template <concepts::SupportFromDynamic T>
    [[nodiscard]] inline operator T() & {
        T v;
        detail::fromDynamicInternal(*this, v);
        return std::forward<T>(v);
    }
    template <concepts::SupportFromDynamic T>
    [[nodiscard]] inline operator T() && {
        T v;
        detail::fromDynamicInternal(*this, v);
        return std::forward<T>(v);
    }

    static inline DynamicValue object() { return DynamicValue{ObjectType{}}; }
    static inline DynamicValue array() { return DynamicValue{ArrayType{}}; }

    template <ll::concepts::IsOneOf<AllElementTypes, ElementType, ObjectType, ArrayType> T>
    [[nodiscard]] inline bool hold() const noexcept {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return hold<ElementType>() && std::holds_alternative<T>(std::get<ElementType>(*this));
        } else {
            return std::holds_alternative<T>(*this);
        }
    }
    template <ll::concepts::IsOneOf<AllElementTypes, ElementType, ObjectType, ArrayType> T>
    [[nodiscard]] inline T const& get() const& {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    template <ll::concepts::IsOneOf<AllElementTypes, ElementType, ObjectType, ArrayType> T>
    [[nodiscard]] inline T& get() & {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    template <ll::concepts::IsOneOf<AllElementTypes, ElementType, ObjectType, ArrayType> T>
    [[nodiscard]] inline T&& get() && {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(std::move(*this)));
        } else {
            return std::get<T>(std::move(*this));
        }
    }
    template <ll::concepts::IsOneOf<AllElementTypes, ElementType, ObjectType, ArrayType> T>
    [[nodiscard]] inline optional_ref<T> tryGet() noexcept {
        if (!hold<T>()) return {};
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    [[nodiscard]] inline bool is_array() const noexcept { return hold<ArrayType>(); }
    [[nodiscard]] inline bool is_boolean() const noexcept { return hold<bool>(); }
    [[nodiscard]] inline bool is_null() const noexcept { return hold<nullptr_t>(); }
    [[nodiscard]] inline bool is_object() const noexcept { return hold<ObjectType>(); }
    [[nodiscard]] inline bool is_string() const noexcept { return hold<std::string>(); }
    [[nodiscard]] inline bool is_number() const noexcept { return hold<NumberType>(); }
    [[nodiscard]] inline bool is_structured() const noexcept { return is_array() || is_object(); }

    [[nodiscard]] inline size_t size() const noexcept {
        if (hold<ElementType>()) {
            if (is_null()) return 0;
            return 1;
        }
        if (is_object()) return get<ObjectType>().size();
        if (is_array()) return get<ArrayType>().size();
        return 0;
    }
    // Object
    [[nodiscard]] inline bool contains(std::string const& key) const noexcept {
        if (!hold<ObjectType>()) return false;
        return get<ObjectType>().contains(key);
    }
    [[nodiscard]] inline DynamicValue const& operator[](std::string const& index) const {
        if (!hold<ObjectType>()) {
            throw std::runtime_error("value not hold an object");
        }
        return get<ObjectType>().at(index);
    }
    [[nodiscard]] inline DynamicValue& operator[](std::string const& index) {
        if (!hold<ObjectType>()) {
            if (hold<NullType>()) {
                return emplace<ObjectType>()[index];
            }
            throw std::runtime_error("value not hold an object");
        }
        return get<ObjectType>()[index];
    }

    [[nodiscard]] inline DynamicValue& operator[](std::string_view index) { return (*this)[std::string{index}]; }
    template <size_t N>
    [[nodiscard]] inline DynamicValue& operator[](const char (&index)[N]) {
        return (*this)[std::string{index, N}];
    }
    [[nodiscard]] inline auto const& items() const {
        if (!hold<ObjectType>()) {
            throw std::runtime_error("value not hold an object");
        }
        return get<ObjectType>();
    }
    [[nodiscard]] inline auto& items() {
        if (!hold<ObjectType>()) {
            throw std::runtime_error("value not hold an object");
        }
        return get<ObjectType>();
    }
    // Array
    [[nodiscard]] inline DynamicValue const& operator[](size_t index) const {
        if (!is_array()) {
            throw std::runtime_error("value not hold an array");
        }
        return get<ArrayType>().at(index);
    }
    [[nodiscard]] inline DynamicValue& operator[](size_t index) {
        if (!is_array()) {
            throw std::runtime_error("value not hold an array");
        }
        return get<ArrayType>()[index];
    }
    template <typename... Args>
        requires(requires(Args... args) { DynamicValue(std::forward<Args>(args)...); })
    inline DynamicValue& emplace_back(Args... args) {
        if (!is_array()) {
            if (is_null()) return emplace<ArrayType>().emplace_back(args...);
            throw std::runtime_error("");
        }
        return get<ArrayType>().emplace_back(args...);
    }
    [[nodiscard]] inline operator std::string const&() const { return get<std::string>(); };
    // [[nodiscard]] inline operator std::string&() & { return get<std::string>(); };
    [[nodiscard]] inline operator std::string&&() && { return std::move(get<std::string>()); };
    [[nodiscard]] inline operator std::string_view() const { return get<std::string>(); };
};

template <concepts::SupportToDynamic T>
[[nodiscard]] inline ll::Expected<> toDynamic(DynamicValue& dv, T&& val){
    return detail::toDynamicInternal(dv, std::forward<T>(val));
};
template <concepts::SupportFromDynamic T>
[[nodiscard]] inline ll::Expected<> fromDynamic(DynamicValue& dv, T& t){
    return detail::fromDynamicInternal(dv, t);
};


// NOLINTEND: google-explicit-constructor
} // namespace remote_call
