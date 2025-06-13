#include "Test.h"
#include "ll/api/Expected.h"
#include "mc/world/item/ItemStack.h"
#include "reflection/SerializationExt.h"
#include "remote_call/api/API.h"

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
// static_assert(FullSupported<Vec3 const*>);

static_assert(FullSupported<Vec3>);
static_assert(FullSupported<BlockPos>);
static_assert(FullSupported<std::pair<BlockPos, int>>);
static_assert(FullSupported<std::pair<Vec3, char>>);
static_assert(FullSupported<std::pair<Vec3, DimensionType>>);

static_assert(FullSupported<Actor*>);
static_assert(SupportToDynamic<Actor&>);
static_assert(FullSupported<std::reference_wrapper<CompoundTag>>);

// static_assert(FullSupported<Mob*>);
static_assert(FullSupported<Player*>);
static_assert(FullSupported<ServerPlayer*>);
// static_assert(FullSupported<SimulatedPlayer const*>);

static_assert(FullSupported<Block const*>);
static_assert(FullSupported<BlockActor*>);
static_assert(FullSupported<ItemStack const*>);
static_assert(FullSupported<std::unique_ptr<ItemStack>>);
static_assert(FullSupported<Container*>);

static_assert(FullSupported<CompoundTag*>);
static_assert(FullSupported<std::unique_ptr<CompoundTag>>);

static_assert(FullSupported<std::vector<Vec3>>);
static_assert(FullSupported<std::array<Vec3, 10>>);
static_assert(FullSupported<std::unordered_map<Tag::Type, std::unique_ptr<CompoundTag>>>);

void test1(DynamicValue dv, std::tuple<Vec3, DimensionType> v) {
    (void)dv.getTo(v);
    (void)dv.tryGet<decltype(v)>();
    (void)DynamicValue::from(v);
    (void)remote_call::deserializeImpl(dv, v, priority::Hightest);
    (void)remote_call::toDynamic(dv, v);
    (void)remote_call::fromDynamic(dv, v);
    (void)DynamicValue{v};
    // v = dv = v;
    // v = dv["key"] = v;
    // v = dv[4] = v;
    (void)remote_call::exportAs("", "", [](Actor&) {});
    (void)remote_call::importAs<void(Player&)>("", "");
};
} // namespace remote_call::test


class StaticCustomType : public std::variant<std::string, ItemInstance> {
    std::string customName;

public:
    explicit StaticCustomType(std::string id) : std::variant<std::string, ItemInstance>(id) {}
    explicit StaticCustomType(ItemInstance item) : std::variant<std::string, ItemInstance>(item) {}
};


ll::Expected<StaticCustomType>
fromDynamic(remote_call::DynamicValue& dv, std::in_place_type_t<StaticCustomType>, remote_call::priority::HightTag) {
    if (dv.hold<std::string>()) {
        return StaticCustomType(dv.get<std::string>());
    } else if (dv.hold<remote_call::ItemType>()) {
        return StaticCustomType(ItemInstance(*dv.get<remote_call::ItemType>().ptr));
    } else {
        return ll::makeStringError("Invalid type");
    }
}

template <typename T>
    requires(std::same_as<std::decay_t<T>, StaticCustomType>)
ll::Expected<> toDynamic(remote_call::DynamicValue& dv, T&& v, remote_call::priority::HightTag) {
    if (std::holds_alternative<std::string>(v)) {
        return remote_call::DynamicValue::from(dv, std::get<std::string>(std::forward<T>(v)));
    } else if (std::holds_alternative<ItemInstance>(v)) {
        return remote_call::DynamicValue::from(
            dv,
            std::make_unique<ItemStack>(std::get<ItemInstance>(std::forward<T>(v)))
        );
    } else {
        return ll::makeStringError("Invalid type");
    }
}

struct CustomTypeWrapper {
    std::string      name;
    StaticCustomType customType;
};
namespace remote_call::test {

static_assert(FullSupported<StaticCustomType>);
static_assert(FullSupported<std::optional<CustomTypeWrapper>>);
static_assert(FullSupported<std::vector<CustomTypeWrapper>>);
static_assert(FullSupported<std::array<CustomTypeWrapper, 10>>);
static_assert(FullSupported<std::unordered_map<std::string, CustomTypeWrapper>>);
static_assert(FullSupported<std::tuple<std::string, CustomTypeWrapper>>);
inline void testConvert(DynamicValue dv, [[maybe_unused]] ll::Expected<> res) {
    // fromDynamic1(dv, std::in_place_type<std::optional<StaticCustomType>>, priority::Hightest);
    (void)dv.tryGet<std::optional<StaticCustomType>>();
    (void)dv.tryGet<std::vector<StaticCustomType>>();
    (void)dv.tryGet<std::array<StaticCustomType, 10>>();
    (void)dv.tryGet<std::tuple<StaticCustomType, int>>();
    (void)dv.tryGet<std::unordered_map<std::string, StaticCustomType>>();

    (void)dv.tryGet<std::optional<CustomTypeWrapper>>();
    (void)dv.tryGet<std::vector<CustomTypeWrapper>>();
    (void)dv.tryGet<std::array<CustomTypeWrapper, 10>>();
    (void)dv.tryGet<std::tuple<CustomTypeWrapper, int>>();
    (void)dv.tryGet<std::unordered_map<std::string, CustomTypeWrapper>>();

    (void)dv.tryGet<std::optional<StaticCustomType>>(res);
    (void)dv.tryGet<std::vector<StaticCustomType>>(res);
    (void)dv.tryGet<std::array<StaticCustomType, 10>>(res);
    (void)dv.tryGet<std::tuple<StaticCustomType, int>>(res);
    (void)dv.tryGet<std::unordered_map<std::string, StaticCustomType>>(res);
    (void)dv.tryGet<std::optional<CustomTypeWrapper>>(res);
    (void)dv.tryGet<std::vector<CustomTypeWrapper>>(res);
    (void)dv.tryGet<std::array<CustomTypeWrapper, 10>>(res);
    (void)dv.tryGet<std::tuple<CustomTypeWrapper, int>>(res);
    (void)dv.tryGet<std::unordered_map<std::string, CustomTypeWrapper>>(res);

    std::any any;
    (void)DynamicValue::from(std::any_cast<std::optional<StaticCustomType>>(any));
    (void)DynamicValue::from(std::any_cast<std::optional<CustomTypeWrapper>>(any));
    (void)DynamicValue::from(std::any_cast<std::vector<CustomTypeWrapper>>(any));
    (void)DynamicValue::from(std::any_cast<std::array<CustomTypeWrapper, 10>>(any));
    (void)DynamicValue::from(std::any_cast<std::tuple<CustomTypeWrapper, int>>(any));
    (void)DynamicValue::from(std::any_cast<std::unordered_map<std::string, CustomTypeWrapper>>(any));
    success("");
};

// import/export
template <typename Fn>
constexpr void exportImportTest(void* any = nullptr) {
    remote_call::exportAs("", "", std::forward<Fn>(*reinterpret_cast<std::decay_t<Fn>>(any)));
    (void)remote_call::importAs<Fn>("", "");
}
inline void test111([[maybe_unused]] void* any) {
    exportImportTest<void(Player&)>();
    // remote_call::exportAs("", "", std::forward<void(std::unique_ptr<CompoundTag>&,
    // int)>(*(std::decay_t<void(std::unique_ptr<CompoundTag>&, int)>)any));
    (void)remote_call::importAs<void(std::unique_ptr<CompoundTag>&, int)>("", "");

    // exportImportTest<void(std::unique_ptr<CompoundTag>&, int)>();
    exportImportTest<Player&(Actor*, int, std::string)>();
    // exportImportTest<std::string const&(Actor*, int, std::string, std::vector<StaticCustomType>)>();
    // exportImportTest<ll::Expected<int>(Actor*, int, std::string, std::vector<CustomTypeWrapper>)>();
    // exportImportTest<ll::Expected<std::tuple<StaticCustomType>>(Actor*, int, std::string,
    // std::vector<CustomTypeWrapper>)>();
};


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
    else dv.emplace<DynamicElement>(NULL_VALUE);
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

} // namespace remote_call::test
