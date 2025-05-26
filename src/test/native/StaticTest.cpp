#include "Test.h"

#include "reflection/SerializationExt.h"
#include "remote_call/api/RemoteCall.h"
#include "remote_call/api/value/DynamicValue.h"

#include <any>


namespace remote_call::test {

using remote_call::concepts::SupportFromDynamic;
using remote_call::concepts::SupportToDynamic;

template <typename T>
concept FullSupported = SupportToDynamic<T> && SupportFromDynamic<T>;


// static_assert(FullSupported<void>);
static_assert(FullSupported<uchar>);
static_assert(FullSupported<short>);
static_assert(FullSupported<int>);
static_assert(FullSupported<bool>);
static_assert(FullSupported<float>);
static_assert(FullSupported<double>);
static_assert(FullSupported<size_t>);
static_assert(FullSupported<std::string>);
static_assert(FullSupported<std::optional<std::string>>);
static_assert(FullSupported<std::nullptr_t>);

static_assert(FullSupported<Vec3>);
static_assert(FullSupported<Vec3&>);
// static_assert(FullSupported<Vec3 const*>);

static_assert(FullSupported<Vec3>);
static_assert(FullSupported<BlockPos>);
static_assert(FullSupported<std::pair<BlockPos, int>>);
static_assert(FullSupported<std::pair<Vec3, char>>);
static_assert(FullSupported<std::pair<Vec3, DimensionType>>);

static_assert(FullSupported<Actor*>);
// static_assert(FullSupported<Mob*>);
static_assert(FullSupported<Player*>);
static_assert(FullSupported<SimulatedPlayer const*>);

static_assert(FullSupported<Block const*>);
static_assert(FullSupported<BlockActor*>);
static_assert(FullSupported<ItemStack const*>);
static_assert(FullSupported<std::unique_ptr<ItemStack>>);
static_assert(FullSupported<Container*>);

static_assert(FullSupported<CompoundTag*>);
static_assert(FullSupported<std::unique_ptr<CompoundTag>&&>);

static_assert(FullSupported<std::vector<Vec3>>);
static_assert(FullSupported<std::array<Vec3, 10>>);

static_assert(requires(DynamicValue dv, std::tuple<Vec3, DimensionType> v) {
    dv.getTo(v);
    dv.tryGet<decltype(v)>();
    DynamicValue::from(v);
    remote_call::deserializeImpl(dv, v, priority::Hightest);
    remote_call::toDynamic(dv, v);
    remote_call::fromDynamic(dv, v);
    DynamicValue{v};
    // v = dv = v;
    // v = dv["key"] = v;
    // v = dv[4] = v;
    remote_call::exportAs("", "", [](Actor&) {});
    remote_call::importAs<void(Player&)>("", "");
});
} // namespace remote_call::test


class CustomType : std::variant<std::string, ItemInstance> {
    std::string customName;

public:
    explicit CustomType(std::string id) : std::variant<std::string, ItemInstance>(id) {}
    explicit CustomType(ItemInstance item) : std::variant<std::string, ItemInstance>(item) {}
};

#if false
template <>
struct ::remote_call::AdlSerializer<CustomType> {
    inline static ll::Expected<CustomType> fromDynamic(DynamicValue& dv) {
        if (dv.hold<std::string>()) {
            return CustomType(dv.get<std::string>());
        } else if (dv.hold<ItemType>()) {
            return CustomType(ItemInstance(*dv.get<ItemType>().ptr));
        } else {
            return ll::makeStringError("Invalid type");
        }
    }
};
#else
ll::Expected<CustomType>
fromDynamic(remote_call::DynamicValue& dv, std::in_place_type_t<CustomType>, remote_call::priority::HightTag) {
    if (dv.hold<std::string>()) {
        return CustomType(dv.get<std::string>());
    } else if (dv.hold<remote_call::ItemType>()) {
        return CustomType(ItemInstance(*dv.get<remote_call::ItemType>().ptr));
    } else {
        return ll::makeStringError("Invalid type");
    }
}
#endif
struct CustomTypeWrapper {
    std::string name;
    CustomType  customType;
};
namespace remote_call::test {

static_assert(SupportFromDynamic<std::optional<CustomType>>);
static_assert(requires(DynamicValue dv, ll::Expected<> res, std::any any) {
    // fromDynamic1(dv, std::in_place_type<std::optional<CustomType>>, priority::Hightest);
    AdlSerializer<CustomType>::fromDynamic(dv);
    dv.tryGet<std::optional<CustomType>>();
    dv.tryGet<std::optional<CustomType>>(res);
    dv.tryGet<std::vector<CustomType>>();
    dv.tryGet<std::vector<CustomType>>(res);
    dv.tryGet<std::array<CustomType, 10>>();
    dv.tryGet<std::array<CustomType, 10>>(res);
    dv.tryGet<std::tuple<CustomType, int>>();
    dv.tryGet<std::tuple<CustomType, int>>(res);
    dv.tryGet<std::unordered_map<std::string, CustomType>>();
    dv.tryGet<std::unordered_map<std::string, CustomType>>(res);

    AdlSerializer<CustomTypeWrapper>::fromDynamic(dv);
    dv.tryGet<std::optional<CustomTypeWrapper>>();
    dv.tryGet<std::optional<CustomTypeWrapper>>(res);
    dv.tryGet<std::vector<CustomTypeWrapper>>();
    dv.tryGet<std::vector<CustomTypeWrapper>>(res);
    dv.tryGet<std::array<CustomTypeWrapper, 10>>();
    dv.tryGet<std::array<CustomTypeWrapper, 10>>(res);
    dv.tryGet<std::tuple<CustomTypeWrapper, int>>();
    dv.tryGet<std::tuple<CustomTypeWrapper, int>>(res);
    dv.tryGet<std::unordered_map<std::string, CustomTypeWrapper>>();
    dv.tryGet<std::unordered_map<std::string, CustomTypeWrapper>>(res);

    DynamicValue::from(std::any_cast<std::optional<CustomType>>(any));
    DynamicValue::from(std::any_cast<std::optional<CustomTypeWrapper>>(any));
    DynamicValue::from(std::any_cast<std::vector<CustomTypeWrapper>>(any));
    DynamicValue::from(std::any_cast<std::array<CustomTypeWrapper, 10>>(any));
    DynamicValue::from(std::any_cast<std::tuple<CustomTypeWrapper, int>>(any));
    DynamicValue::from(std::any_cast<std::unordered_map<std::string, CustomTypeWrapper>>(any));
});

template <typename T>
concept IsPointer = std::is_pointer_v<T>;

template <typename T>
concept IsOptionalRef = requires(std::decay_t<T> v) {
    { v.as_ptr() } -> IsPointer;
    v.has_value();
    v = v.as_ptr();
};
static_assert(IsOptionalRef<optional_ref<Actor>>);

template <IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
ll::Expected<> toDynamic1(DynamicValue& dv, T&& t, priority::LowTag) {
    if (t.has_value()) return ::remote_call::toDynamic(dv, std::forward<T>(t).as_ptr());
    else dv.emplace<ElementType>(NULL_VALUE);
    return {};
}
template <IsOptionalRef T>
    requires(concepts::SupportFromDynamic<decltype(std::declval<T>().as_ptr())>)
ll::Expected<> fromDynamic1(DynamicValue& dv, T& t, priority::DefaultTag) {
    if (dv.hold<NullType>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = ::remote_call::fromDynamic(dv, val);
        if (res) t = val;
        return res;
    }
}
static_assert(requires(DynamicValue dv, optional_ref<Actor>& v) {
    toDynamic1(dv, std::forward<decltype(v)>(v), priority::Hightest);
    fromDynamic1(dv, std::forward<decltype(v)>(v), priority::Hightest);
});

static_assert(!concepts::IsString<nullptr_t>);
static_assert(concepts::IsTupleLike<std::tuple<>>);
static_assert(requires(std::string& str, remote_call::DynamicValue& dv) {
    str = std::string{std::forward<remote_call::DynamicValue>(dv)};
});

} // namespace remote_call::test
