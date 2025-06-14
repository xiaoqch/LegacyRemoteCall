#include "Test.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Containers.h"
#include "mc/world/Container.h"
#include "mc/world/item/ItemStack.h"
#include "remote_call/api/API.h"

#include <any>

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

struct StaticCustomTypeWrapper {
    std::string      name;
    StaticCustomType customType;
};

// clang-format off
template <> struct std::hash<StaticCustomType> { constexpr size_t operator()(StaticCustomType const&) const { return 0; } };
template <> struct std::less<StaticCustomType> { constexpr bool operator()(StaticCustomType const&, StaticCustomType const&) const { return true; } };
template <> struct std::equal_to<StaticCustomType> { constexpr bool operator()(StaticCustomType const&, StaticCustomType const&) const { return true; } };
template <> struct std::hash<StaticCustomTypeWrapper> { constexpr size_t operator()(StaticCustomTypeWrapper const&) const { return 0; } };
template <> struct std::less<StaticCustomTypeWrapper> { constexpr bool operator()(StaticCustomTypeWrapper const&, StaticCustomTypeWrapper const&) const { return true; } };
template <> struct std::equal_to<StaticCustomTypeWrapper> { constexpr bool operator()(StaticCustomTypeWrapper const&, StaticCustomTypeWrapper const&) const { return true; } };
// clang-format on


namespace remote_call::test {

using remote_call::concepts::SupportFromDynamic;
using remote_call::concepts::SupportToDynamic;

template <typename T, template <typename A> class... Ctn>
using wrap_list_t = std::tuple<Ctn<T>...>;
template <typename T, template <typename K, typename V> class... Ctn>
using wrap_map_t = std::tuple<Ctn<std::string, T>...>;

template <typename T = StaticCustomType>
inline void testForType(DynamicValue& dv, T& v) {
    (void)dv.getTo(v).value();
    (void)dv.tryGet<T>().value();
    (void)DynamicValue::from(v).value();
    (void)remote_call::toDynamic(dv, v).value();
    if constexpr (std::default_initializable<T>) {
        (void)remote_call::fromDynamic(dv, v).value();
    } else {
        (void)remote_call::fromDynamic(dv, std::in_place_type<T>).value();
    }

    (void)remote_call::exportAs("", "", [&](T&) -> T { return v; }).value();
    (void)remote_call::importAs<T(T&)>("", "")(v).value();
}

[[maybe_unused]] inline void testCustom(DynamicValue& dv) {
    StaticCustomType        sct{""};
    StaticCustomTypeWrapper w{"", {sct}};

    wrap_list_t<StaticCustomType, std::vector, std::set, std::unordered_set, ll::SmallDenseSet, ll::DenseSet> lists;
    wrap_map_t<
        StaticCustomType,
        std::map,
        ll::DenseMap,
        ll::SmallDenseMap,
        ll::DenseNodeMap,
        ll::SmallDenseNodeMap,
        std::unordered_map>
                                                                                                       maps;
    wrap_list_t<StaticCustomTypeWrapper, std::vector, std::set, std::unordered_set, ll::SmallDenseSet> wls;
    wrap_map_t<
        StaticCustomTypeWrapper,
        std::map,
        ll::DenseMap,
        ll::SmallDenseMap,
        ll::DenseNodeMap,
        ll::SmallDenseNodeMap,
        std::unordered_map>
        wms;

    testForType(dv, sct);
    testForType(dv, w);
    testForType(dv, lists);
    testForType(dv, maps);
    testForType(dv, wls);
    testForType(dv, wms);
}


[[maybe_unused]] inline void testAllCompile(DynamicValue dv) {
    using SimpleTypes = std::tuple<
        uchar,
        short,
        int,
        bool,
        float,
        double,
        size_t,
        std::string,
        std::optional<std::string>,
        std::nullptr_t,
        std::monostate>;
    using ExtraTypes = std::tuple<
        Vec3,
        BlockPos,
        std::pair<BlockPos, int>,
        std::pair<Vec3, char>,
        std::pair<Vec3, DimensionType>,
        Actor*,
        // std::reference_wrapper<Container>,
        Player*,
        ServerPlayer const*,
        Block const*,
        BlockActor*,
        ItemStack const*,
        // std::unique_ptr<ItemStack>,
        Container*,
        // std::unique_ptr<CompoundTag>,
        // StaticCustomTypeWrapper,
        CompoundTag*>;
    using ElementTypes = std::pair<SimpleTypes, ExtraTypes>;
    using Lists        = wrap_list_t<
               ElementTypes,
               ll::DenseSet,
               ll::SmallDenseSet,
               ll::ConcurrentDenseSet,
               // std::set,
               // std::unordered_set,
               std::vector>;
    using Maps = wrap_map_t<
        ElementTypes,
        std::map,
        ll::DenseMap,
        ll::SmallDenseMap,
        ll::DenseNodeMap,
        ll::SmallDenseNodeMap,
        std::unordered_map>;

    using Containers = std::tuple<Lists, Maps>;

    [[maybe_unused]] SimpleTypes simple = std::any_cast<SimpleTypes>(std::any{});
    [[maybe_unused]] ExtraTypes  extra  = std::any_cast<ExtraTypes>(std::any{});
    [[maybe_unused]] Lists       list   = std::any_cast<Lists>(std::any{});
    [[maybe_unused]] Maps        map    = std::any_cast<Maps>(std::any{});
    [[maybe_unused]] Containers  ctn    = std::any_cast<Containers>(std::any{});
    // testForType(dv, list);
    auto&& v = ctn;
    using T  = std::decay_t<decltype(v)>;
    (void)dv.getTo(static_cast<T&>(v)).value();
    (void)dv.tryGet<T>().value();
    (void)DynamicValue::from(static_cast<T&&>(v)).value();
    (void)remote_call::toDynamic(dv, static_cast<T&&>(v)).value();
    (void)remote_call::fromDynamic(dv, static_cast<T&>(v)).value();

    (void)remote_call::exportAs("", "", [](T&) -> T { return T{}; }).value();
    (void)remote_call::importAs<T(T&)>("", "")(static_cast<T&>(v)).value();
};

[[maybe_unused]] inline void testConvert(DynamicValue dv, [[maybe_unused]] ll::Expected<> res) {
    // fromDynamic1(dv, std::in_place_type<std::optional<StaticCustomType>>, priority::Hightest);
    (void)dv.tryGet<std::optional<StaticCustomType>>();
    (void)dv.tryGet<std::vector<StaticCustomType>>();
    (void)dv.tryGet<std::array<StaticCustomType, 10>>();
    (void)dv.tryGet<std::tuple<StaticCustomType, int>>();
    (void)dv.tryGet<std::unordered_map<std::string, StaticCustomType>>();

    (void)dv.tryGet<std::optional<StaticCustomTypeWrapper>>();
    (void)dv.tryGet<std::vector<StaticCustomTypeWrapper>>();
    (void)dv.tryGet<std::array<StaticCustomTypeWrapper, 10>>();
    (void)dv.tryGet<std::tuple<StaticCustomTypeWrapper, int>>();
    (void)dv.tryGet<std::unordered_map<std::string, StaticCustomTypeWrapper>>();

    (void)dv.tryGet<std::optional<StaticCustomType>>(res);
    (void)dv.tryGet<std::vector<StaticCustomType>>(res);
    (void)dv.tryGet<std::array<StaticCustomType, 10>>(res);
    (void)dv.tryGet<std::tuple<StaticCustomType, int>>(res);
    (void)dv.tryGet<std::unordered_map<std::string, StaticCustomType>>(res);
    (void)dv.tryGet<std::optional<StaticCustomTypeWrapper>>(res);
    (void)dv.tryGet<std::vector<StaticCustomTypeWrapper>>(res);
    (void)dv.tryGet<std::array<StaticCustomTypeWrapper, 10>>(res);
    (void)dv.tryGet<std::tuple<StaticCustomTypeWrapper, int>>(res);
    (void)dv.tryGet<std::unordered_map<std::string, StaticCustomTypeWrapper>>(res);

    std::any any;
    (void)DynamicValue::from(std::any_cast<std::optional<StaticCustomType>>(any));
    (void)DynamicValue::from(std::any_cast<std::optional<StaticCustomTypeWrapper>>(any));
    (void)DynamicValue::from(std::any_cast<std::vector<StaticCustomTypeWrapper>>(any));
    (void)DynamicValue::from(std::any_cast<std::array<StaticCustomTypeWrapper, 10>>(any));
    (void)DynamicValue::from(std::any_cast<std::tuple<StaticCustomTypeWrapper, int>>(any));
    (void)DynamicValue::from(std::any_cast<std::unordered_map<std::string, StaticCustomTypeWrapper>>(any));
    success("");
};

} // namespace remote_call::test
