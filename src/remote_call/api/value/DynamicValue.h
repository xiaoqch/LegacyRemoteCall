#pragma once

#include "Values.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "mc/deps/core/utility/optional_ref.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/TypeTraits.h"
#include "remote_call/api/conversions/detail/Detail.h"
#include "remote_call/api/value/Base.h"


namespace remote_call {
// NOLINTBEGIN: google-explicit-constructor

namespace error_utils {
template <typename Target, typename... Expected>
[[nodiscard]] LL_NOINLINE LL_CONSTEXPR23 ll::Unexpected makeFromDynamicTypeError(DynamicValue const& value);
}


using detail::DynamicElement;
using detail::NULL_VALUE;
using detail::NullType;
using DynamicObject  = detail::DynamicObjectBase<DynamicValue>;
using DynamicArray   = detail::DynamicArrayBase<DynamicValue>;
using DynamicVariant = detail::DynamicVariantBase<DynamicValue, DynamicElement>;

template <typename T>
concept IsAnyDynamicElement = ll::concepts::IsOneOf<T, AllElementTypes, DynamicElement, DynamicObject, DynamicArray>;

struct DynamicValue : public detail::DynamicBase<DynamicVariant> {
    using BaseType = detail::DynamicBase<DynamicVariant>;

    constexpr DynamicValue() noexcept : BaseType(DynamicElement{NULL_VALUE}) {};
    constexpr DynamicValue(const DynamicValue&)                                                             = delete;
    constexpr DynamicValue(DynamicValue&&) noexcept(std::is_nothrow_move_constructible_v<BaseType>)         = default;
    constexpr DynamicValue& operator=(const DynamicValue&)                                                  = delete;
    constexpr DynamicValue& operator=(DynamicValue&&) noexcept(std::is_nothrow_move_assignable_v<BaseType>) = default;
    template <typename T>
        requires(!std::is_reference_v<T> && traits::is_variant_alternative_v<DynamicVariant, T>)
    constexpr explicit DynamicValue(T&& v) noexcept(std::is_nothrow_constructible_v<BaseType, T&&>)
    : BaseType(std::forward<T>(v)){};
    template <typename T>
        requires(!std::is_reference_v<T> && traits::is_variant_alternative_v<DynamicVariant, T>)
    constexpr DynamicValue& operator=(T&& v) noexcept(std::is_nothrow_assignable_v<BaseType, T&&>) {
        this->emplace<T>(std::forward<T>(v));
        return *this;
    }
    template <typename T>
        requires(requires(T&& v) { DynamicElement(std::forward<T>(v)); })
    constexpr DynamicValue(T&& v) noexcept(std::is_nothrow_constructible_v<DynamicElement, T&&>)
    : BaseType(DynamicElement(std::forward<T>(v))){};
    template <concepts::SupportToDynamic T>
        requires(!requires(T&& v) {
            DynamicElement(std::forward<T>(v));
        } && !std::same_as<T, DynamicValue> && !IsAnyDynamicElement<T>)
    LL_CONSTEXPR23 DynamicValue(T&& v) : BaseType() {
        ::remote_call::toDynamic(*this, std::forward<T>(v)).value();
    };
    // template <concepts::SupportFromDynamic T>
    // [[nodiscard]] constexpr operator T() & {
    //     T v;
    //     ::remote_call::fromDynamic(*this, v);
    //     return std::forward<T>(v);
    // }
    // template <concepts::SupportFromDynamic T>
    // [[nodiscard]] constexpr operator T() && {
    //     T v;
    //     ::remote_call::fromDynamic(*this, v);
    //     return std::forward<T>(v);
    // }

    template <ll::concepts::IsOneOf<AllElementTypes> T, class... Args>
    LL_FORCEINLINE constexpr T& emplace(Args&&... val) noexcept(std::is_nothrow_constructible_v<T, Args&&...>) {
        return BaseType::emplace<DynamicElement>().template emplace<T>(std::forward<Args>(val)...);
    }
    template <ll::concepts::IsOneOf<DynamicElement, DynamicArray, DynamicObject> T, class... Args>
    LL_FORCEINLINE constexpr T& emplace(Args&&... val) noexcept(std::is_nothrow_constructible_v<T, Args&&...>) {
        return BaseType::emplace<T>(std::forward<Args>(val)...);
    }

    [[nodiscard]] LL_FORCEINLINE static DynamicValue object() noexcept { return DynamicValue{DynamicObject{}}; }
    [[nodiscard]] constexpr static DynamicValue      array() noexcept { return DynamicValue{DynamicArray{}}; }

    template <IsAnyDynamicElement T>
    [[nodiscard]] LL_FORCEINLINE constexpr bool hold() const noexcept {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return hold<DynamicElement>() && std::holds_alternative<T>(std::get<DynamicElement>(*this));
        } else {
            return std::holds_alternative<T>(*this);
        }
    }
    template <IsAnyDynamicElement T>
    [[nodiscard]] LL_FORCEINLINE constexpr T const& get() const& {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<DynamicElement>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    template <IsAnyDynamicElement T>
    [[nodiscard]] LL_FORCEINLINE constexpr T& get() & {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<DynamicElement>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    template <IsAnyDynamicElement T>
    [[nodiscard]] LL_FORCEINLINE constexpr T&& get() && {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<DynamicElement>(std::move(*this)));
        } else {
            return std::get<T>(std::move(*this));
        }
    }

    template <IsAnyDynamicElement T>
    [[nodiscard]] inline ll::Expected<> getTo(T& out) {
        if constexpr (std::is_pointer_v<T>) {
            if (is_null()) {
                out = T{};
                return {};
            };
        }
        if (!hold<T>()) {
            return fromDynamic(*this, out);
        } else {
            out = get<T>();
            return {};
        }
    }
    template <concepts::SupportFromDynamicC T>
        requires(!IsAnyDynamicElement<T>)
    [[nodiscard]] inline ll::Expected<> getTo(T& out) {
        using Type = std::decay_t<decltype(*fromDynamic(*this, std::in_place_type<T>))>;
        return fromDynamic(*this, std::in_place_type<T>).and_then([&](Type&& val) -> ll::Expected<> {
            out = std::forward<decltype(val)>(val);
            return {};
        });
    }
    template <concepts::SupportFromDynamicR T>
        requires(!IsAnyDynamicElement<T>)
    [[nodiscard]] inline ll::Expected<> getTo(T& out) {
        return fromDynamic(*this, out);
    }
    template <std::same_as<void> T>
    [[nodiscard]] LL_CONSTEXPR23 ll::Expected<T> tryGet() noexcept {
        // if (!is_null()) return error_utils::makeFromDynamicTypeError<T, NullType>(*this);
        return {};
    }
    template <concepts::SupportFromDynamicC T>
        requires(!std::is_lvalue_reference_v<T>)
    [[nodiscard]] LL_CONSTEXPR23 ll::Expected<T> tryGet() {
        return fromDynamic(*this, std::in_place_type<T>);
    }
    template <typename T>
        requires(!std::is_reference_v<T> && !concepts::SupportFromDynamicC<T> && concepts::SupportFromDynamicR<T>)
    [[nodiscard]] LL_CONSTEXPR23 ll::Expected<T> tryGet() {
        T v{};
        return fromDynamic(*this, v).transform([&]() { return std::move(v); });
    }
    template <typename T>
        requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<traits::reference_to_wrapper_t<T>>)
    [[nodiscard]] LL_CONSTEXPR23 ll::Expected<traits::reference_to_wrapper_t<T>> tryGet() {
        return tryGet<traits::reference_to_wrapper_t<T>>();
    }
    template <typename T>
        requires((std::is_rvalue_reference_v<T> || (std::is_lvalue_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>)) && concepts::SupportFromDynamic<std::decay_t<T>>)
    [[nodiscard]] LL_CONSTEXPR23 ll::Expected<std::decay_t<T>> tryGet() {
        return tryGet<std::decay_t<T>>();
    }

    template <concepts::SupportFromDynamicC T>
    [[nodiscard]] constexpr std::optional<T> tryGet(ll::Expected<>& success) {
        auto result = tryGet<T>();
        if (result) return *std::move(result);
        success = ll::forwardError(result.error());
        return {};
    }
    template <concepts::SupportFromDynamicR T>
        requires(!concepts::SupportFromDynamicC<T>)
    [[nodiscard]] constexpr std::optional<T> tryGet(ll::Expected<>& success) {
        if (success) {
            T v{};
            success = getTo(v);
            if (success) return v;
        }
        return {};
    }
    template <typename T>
        requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<traits::reference_to_pointer_t<T>>)
    [[nodiscard]] constexpr optional_ref<std::remove_reference_t<T>> tryGet(ll::Expected<>& success) {
        if (success) {
            traits::reference_to_pointer_t<T> ptr{};
            success = getTo(ptr);
            if (success) return ptr;
        }
        return {};
    }
    template <typename T>
    [[nodiscard]] inline static ll::Expected<> from(DynamicValue& dv, T&& v) {
        return toDynamic(dv, std::forward<T>(v));
    }

    template <typename T>
    [[nodiscard]] inline static ll::Expected<DynamicValue> from(T&& v) {
        DynamicValue dv{NULL_VALUE};
        return toDynamic(dv, std::forward<T>(v)).transform([&]() { return std::move(dv); });
    }

    [[nodiscard]] constexpr bool is_array() const noexcept { return hold<DynamicArray>(); }
    [[nodiscard]] constexpr bool is_boolean() const noexcept { return hold<bool>(); }
    [[nodiscard]] constexpr bool is_null() const noexcept { return hold<nullptr_t>(); }
    [[nodiscard]] constexpr bool is_object() const noexcept { return hold<DynamicObject>(); }
    [[nodiscard]] constexpr bool is_string() const noexcept { return hold<std::string>(); }
    [[nodiscard]] constexpr bool is_number() const noexcept { return hold<NumberType>(); }
    [[nodiscard]] constexpr bool is_structured() const noexcept { return is_array() || is_object(); }

    [[nodiscard]] constexpr size_t size() const noexcept {
        if (hold<DynamicElement>()) {
            if (is_null()) return 0;
            return 1;
        }
        if (is_object()) return get<DynamicObject>().size();
        if (is_array()) return get<DynamicArray>().size();
        return 0;
    }
    // Object
    [[nodiscard]] constexpr bool contains(std::string const& key) const noexcept {
        if (!hold<DynamicObject>()) return false;
        return get<DynamicObject>().contains(key);
    }
    [[nodiscard]] inline DynamicValue const& operator[](std::string const& index) const {
        if (!hold<DynamicObject>()) {
            throw std::runtime_error("value not hold an object");
        }
        return get<DynamicObject>().at(index);
    }
    [[nodiscard]] constexpr DynamicValue& operator[](std::string const& index) {
        if (!hold<DynamicObject>()) {
            if (hold<NullType>()) {
                return emplace<DynamicObject>()[index];
            }
            throw std::runtime_error("value not hold an object");
        }
        return get<DynamicObject>()[index];
    }

    [[nodiscard]] constexpr DynamicValue& operator[](std::string_view index) { return (*this)[std::string{index}]; }
    template <size_t N>
    [[nodiscard]] constexpr DynamicValue& operator[](const char (&index)[N]) {
        return (*this)[std::string_view{index, N - 1}];
    }
    [[nodiscard]] constexpr auto const& items() const {
        if (!hold<DynamicObject>()) {
            throw std::runtime_error("value not hold an object");
        }
        return get<DynamicObject>();
    }
    [[nodiscard]] constexpr auto& items() {
        if (!hold<DynamicObject>()) {
            throw std::runtime_error("value not hold an object");
        }
        return get<DynamicObject>();
    }
    // Array
    [[nodiscard]] constexpr DynamicValue const& operator[](size_t index) const {
        if (!is_array()) {
            throw std::runtime_error("value not hold an array");
        }
        return get<DynamicArray>().at(index);
    }
    [[nodiscard]] constexpr DynamicValue& operator[](size_t index) {
        if (!is_array()) {
            throw std::runtime_error("value not hold an array");
        }
        return get<DynamicArray>()[index];
    }
    template <typename... Args>
        requires(requires(Args... args) { DynamicValue(std::forward<Args>(args)...); })
    constexpr DynamicValue& emplace_back(Args... args) {
        if (!is_array()) {
            if (is_null()) return emplace<DynamicArray>().emplace_back(args...);
            throw std::runtime_error("");
        }
        return get<DynamicArray>().emplace_back(args...);
    }
    [[nodiscard]] constexpr operator std::string const&() const { return get<std::string>(); };
    // [[nodiscard]] constexpr operator std::string&() & { return get<std::string>(); };
    [[nodiscard]] constexpr operator std::string&&() && { return std::move(get<std::string>()); };
    [[nodiscard]] constexpr operator std::string_view() const { return get<std::string>(); };
};

// NOLINTEND: google-explicit-constructor
} // namespace remote_call
