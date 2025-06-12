#include "Test.h"
#include "ll/api/Expected.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/reflection/Reflection.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/utils/ErrorUtils.h"
#include "remote_call/api/API.h" // IWYU pragma: keep

// variant
namespace remote_call::detail {
inline ll::Expected<>
fromDynamic(remote_call::DynamicValue& dv, std::variant<std::string, int>& v, remote_call::priority::HightTag) {
    if (dv.hold<std::string>()) {
        v.emplace<std::string>(dv.get<std::string>());
        return {};
    } else if (dv.hold<remote_call::ItemType>()) {
        v.emplace<int>(dv.get<remote_call::NumberType>().get<int>());
        return {};
    } else {
        return remote_call::error_utils::
            makeFromDynamicTypeError<std::variant<std::string, int>, std::string, remote_call::ItemType>(dv);
    }
}

template <typename Variant>
inline ll::Expected<> toDynamic(remote_call::DynamicValue& dv, Variant&& v, remote_call::priority::HightTag)
    requires(requires() {
        (void)std::visit(
            [](auto&& val) { static_assert(remote_call::concepts::SupportToDynamic<decltype(val)>); },
            std::forward<decltype(v)>(v)
        );
    })
{
    return std::visit(
        [&](auto&& val) { return remote_call::DynamicValue::from(dv, std::forward<decltype(val)>(val)); },
        std::forward<decltype(v)>(v)
    );
}
} // namespace remote_call::detail

class CustomType : std::variant<std::string, int> {

public:
    std::string customName;
    explicit CustomType(std::string id) : std::variant<std::string, int>(id) {}
    explicit CustomType(int identifier) : std::variant<std::string, int>(identifier) {}
    explicit CustomType(std::variant<std::string, int> identifier)
    : std::variant<std::string, int>(std::move(identifier)) {}

    bool operator==(CustomType const& rhs) const = default;
};

struct CustomTypeWrapper {
    std::string name;
    CustomType  customType;
    bool        operator==(CustomTypeWrapper const& rhs) const = default;
};

static_assert(ll::reflection::Reflectable<CustomTypeWrapper>);
static_assert(!std::is_constructible_v<CustomTypeWrapper>);

struct TempCustomType {
    std::variant<std::string, int> identifier;
    std::optional<std::string>     name{};

    explicit operator CustomType() && {
        CustomType type{std::move(identifier)};
        if (name) type.customName = *std::move(name);
        return type;
    }
};

template <typename T>
    requires(std::same_as<std::decay_t<T>, CustomType>)
inline ll::Expected<> toDynamic(remote_call::DynamicValue& dv, T&& v, remote_call::priority::HightTag) {
    auto& obj = dv.emplace<remote_call::ObjectType>();
    obj["name"].emplace<std::string>(v.customName);
    return remote_call::DynamicValue::from(obj["identifier"], reinterpret_cast<std::variant<std::string, int>&>(v));
}

inline ll::Expected<CustomType>
fromDynamic(remote_call::DynamicValue& dv, std::in_place_type_t<CustomType>, remote_call::priority::HightTag) {
    using Type = std::variant<std::string, int>;
    if (dv.hold<remote_call::ArrayType>()) {
        using Pair = std::pair<Type, std::optional<std::string>>;
        return dv.tryGet<Pair>().transform([](Pair&& pair) {
            CustomType type{std::move(pair).first};
            if (pair.second) {
                type.customName = *pair.second;
            }
            return type;
        });
    } else if (dv.hold<remote_call::ObjectType>()) {
        return dv.tryGet<TempCustomType>().transform([](TempCustomType&& type) {
            return static_cast<CustomType>(std::move(type));
        });
    } else {
        return dv.tryGet<Type>().transform([](Type&& v) {
            return std::visit([](auto&& v) { return CustomType{std::forward<decltype(v)>(v)}; }, std::move(v));
        });
    }
}

namespace remote_call::test {

static_assert(ll::reflection::Reflectable<CustomTypeWrapper>);
static_assert(concepts::SupportFromDynamicC<CustomType>);
static_assert(concepts::SupportFromDynamicC<CustomTypeWrapper>);

static bool testCustomType() {
    CustomTypeWrapper         wrapper{"aaa", static_cast<CustomType>(TempCustomType{"bbb", "ccc"})};
    auto                      res = DynamicValue::from(wrapper)->tryGet<CustomTypeWrapper>().value();
    remote_call::DynamicValue dv;

    assert(res == wrapper);
    return true;
}

[[maybe_unused]] static auto test = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
    [] {
        try {
            if (testCustomType()) {
                success("CustomType Test passed");
            } else {
                error("CustomType Test failed");
            }
        } catch (...) {
            ll::error_utils::printCurrentException(getLogger());
        }
    },
    ll::chrono::ticks{1}
);

} // namespace remote_call::test
