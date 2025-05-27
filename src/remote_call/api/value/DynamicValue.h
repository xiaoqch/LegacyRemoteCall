#pragma once

#include "Values.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "mc/deps/core/utility/optional_ref.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/TypeTraits.h"
#include "remote_call/api/conversions/AdlSerializer.h"
#include "remote_call/api/conversions/detail/Detail.h"
#include "remote_call/api/value/Base.h"


namespace remote_call {
// NOLINTBEGIN: google-explicit-constructor

namespace error_utils {
template <typename Target, typename... Expected>
inline ll::Unexpected makeFromDynamicTypeError(DynamicValue const& value);
}


using detail::ElementType;
using detail::NULL_VALUE;
using detail::NullType;
using ObjectType  = detail::ObjectTypeBase<DynamicValue>;
using ArrayType   = detail::ArrayTypeBase<DynamicValue>;
using VariantType = detail::VariantTypeBase<DynamicValue>;
template <typename T>
concept IsAnyDynamicElement = ll::concepts::IsOneOf<T, AllElementTypes, ElementType, ObjectType, ArrayType>;

struct DynamicValue : public detail::DynamicBase<DynamicValue> {
    using BaseType = detail::DynamicBase<DynamicValue>;

    DynamicValue() : BaseType(ElementType{NULL_VALUE}) {};
    DynamicValue(const DynamicValue&)            = delete;
    DynamicValue(DynamicValue&&)                 = default;
    DynamicValue& operator=(const DynamicValue&) = delete;
    DynamicValue& operator=(DynamicValue&&)      = default;
    template <typename T>
        requires(!std::is_reference_v<T> && traits::is_variant_alternative_v<VariantType, T>)
    explicit DynamicValue(T&& v) : BaseType(std::forward<T>(v)){};
    template <typename T>
        requires(!std::is_reference_v<T> && traits::is_variant_alternative_v<VariantType, T>)
    DynamicValue& operator=(T&& v) {
        this->emplace<T>(std::forward<T>(v));
        return *this;
    }
    template <typename T>
        requires(requires(T&& v) { ElementType(std::forward<T>(v)); })
    DynamicValue(T&& v) : BaseType(ElementType(std::forward<T>(v))){};
    template <concepts::SupportToDynamic T>
        requires(!requires(T&& v) {
            ElementType(std::forward<T>(v));
        } && !std::same_as<T, DynamicValue> && !IsAnyDynamicElement<T>)
    DynamicValue(T&& v) : BaseType() {
        ll::Expected<> res = ::remote_call::toDynamic(*this, std::forward<T>(v));
        if (!res) {
            *this = {NULL_VALUE};
            throw std::runtime_error(res.error().message());
        }
    };
    // template <concepts::SupportFromDynamic T>
    // [[nodiscard]] inline operator T() & {
    //     T v;
    //     ::remote_call::fromDynamic(*this, v);
    //     return std::forward<T>(v);
    // }
    // template <concepts::SupportFromDynamic T>
    // [[nodiscard]] inline operator T() && {
    //     T v;
    //     ::remote_call::fromDynamic(*this, v);
    //     return std::forward<T>(v);
    // }

    static inline DynamicValue object() { return DynamicValue{ObjectType{}}; }
    static inline DynamicValue array() { return DynamicValue{ArrayType{}}; }

    template <IsAnyDynamicElement T>
    [[nodiscard]] inline bool hold() const noexcept {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return hold<ElementType>() && std::holds_alternative<T>(std::get<ElementType>(*this));
        } else {
            return std::holds_alternative<T>(*this);
        }
    }
    template <IsAnyDynamicElement T>
    [[nodiscard]] inline T const& get() const& {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    template <IsAnyDynamicElement T>
    [[nodiscard]] inline T& get() & {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(*this));
        } else {
            return std::get<T>(*this);
        }
    }
    template <IsAnyDynamicElement T>
    [[nodiscard]] inline T&& get() && {
        if constexpr (ll::concepts::IsOneOf<T, AllElementTypes>) {
            return std::get<T>(std::get<ElementType>(std::move(*this)));
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
        if (!hold<T>()) return error_utils::makeFromDynamicTypeError<T, T>(*this);
        out = get<T>();
        return {};
    }
    template <concepts::SupportFromDynamicC T>
        requires(!IsAnyDynamicElement<T>)
    [[nodiscard]] inline ll::Expected<> getTo(T& out) {
        auto res = AdlSerializer<T>::fromDynamic(*this);
        if (!res) return ll::forwardError(std::move(res).error());
        out = std::move(res).value();
        return {};
    }
    template <concepts::SupportFromDynamicR T>
        requires(!IsAnyDynamicElement<T>)
    [[nodiscard]] inline ll::Expected<> getTo(T& out) {
        return AdlSerializer<T>::fromDynamic(*this, out);
    }
    template <std::same_as<void> T>
    [[nodiscard]] inline ll::Expected<T> tryGet() {
        // if (!is_null()) return error_utils::makeFromDynamicTypeError<T, NullType>(*this);
        return {};
    }
    template <concepts::SupportFromDynamicC T>
        requires(!std::is_lvalue_reference_v<T>)
    [[nodiscard]] inline ll::Expected<T> tryGet() {
        return AdlSerializer<T>::fromDynamic(*this);
    }
    template <typename T>
        requires(!std::is_reference_v<T> && !concepts::SupportFromDynamicC<T> && concepts::SupportFromDynamicR<T>)
    [[nodiscard]] inline ll::Expected<T> tryGet() {
        T    v{};
        auto result = AdlSerializer<std::decay_t<T>>::fromDynamic(*this, v);
        if (result) return std::move(v);
        else return ll::forwardError(result.error());
    }
    template <typename T>
        requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<traits::reference_to_wrapper_t<T>>)
    [[nodiscard]] inline ll::Expected<traits::reference_to_wrapper_t<T>> tryGet() {
        return tryGet<traits::reference_to_wrapper_t<T>>();
    }
    template <typename T>
        requires((std::is_rvalue_reference_v<T> || (std::is_lvalue_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>)) && concepts::SupportFromDynamic<std::decay_t<T>>)
    [[nodiscard]] inline ll::Expected<std::decay_t<T>> tryGet() {
        return tryGet<std::decay_t<T>>();
    }

    template <concepts::SupportFromDynamicC T>
    [[nodiscard]] inline std::optional<T> tryGet(ll::Expected<>& success) {
        auto result = tryGet<T>();
        if (result) return *std::move(result);
        success = ll::forwardError(result.error());
        return {};
    }
    template <concepts::SupportFromDynamicR T>
        requires(!concepts::SupportFromDynamicC<T>)
    [[nodiscard]] inline std::optional<T> tryGet(ll::Expected<>& success) {
        T v{};
        if (success) success = getTo(v);
        if (success) return v;
        return {};
    }
    template <typename T>
        requires(std::is_lvalue_reference_v<T> && concepts::SupportFromDynamic<traits::reference_to_pointer_t<T>>)
    [[nodiscard]] inline optional_ref<std::remove_reference_t<T>> tryGet(ll::Expected<>& success) {
        traits::reference_to_pointer_t<T> ptr{};
        if (success) success = getTo(ptr);
        if (success) return ptr;
        return {};
    }
    template <typename T>
    [[nodiscard]] inline static ll::Expected<> from(DynamicValue& dv, T&& v) {
        return AdlSerializer<std::decay_t<T>>::toDynamic(dv, std::forward<T>(v));
    }

    template <typename T>
    [[nodiscard]] inline static ll::Expected<DynamicValue> from(T&& v) {
        ll::Expected<DynamicValue> dv{NULL_VALUE};
        auto                       res = AdlSerializer<std::decay_t<T>>::toDynamic(dv.value(), std::forward<T>(v));
        if (!res) dv = ll::forwardError(res.error());
        return dv;
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
        return (*this)[std::string_view{index, N}];
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

// NOLINTEND: google-explicit-constructor
} // namespace remote_call
