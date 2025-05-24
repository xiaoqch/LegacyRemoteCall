#include "reflection/Deserialization.h"
#include "reflection/Serialization.h"
#include "reflection/SerializationExt.h"
#include "remote_call/api/RemoteCall.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/conversion/DefaultConversion.h"
#include "remote_call/api/value/DynamicValue.h"

#include <vector>


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
static_assert(FullSupported<Vec3 const&>);
// static_assert(FullSupported<Vec3 const*>);

static_assert(FullSupported<Vec3>);
static_assert(FullSupported<BlockPos>);
static_assert(FullSupported<std::pair<BlockPos, int>>);
static_assert(FullSupported<std::pair<Vec3, char>>);
static_assert(FullSupported<std::pair<Vec3, DimensionType>>);

static_assert(FullSupported<Actor*>);
static_assert(FullSupported<Mob*>);
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
    remote_call::serializeImpl(dv, v, priority::Hightest);
    remote_call::deserializeImpl(dv, v, priority::Hightest);
    remote_call::toDynamicImpl(dv, v, ll::meta::PriorityTag<10>{});
    remote_call::fromDynamicImpl(dv, v, priority::Hightest);
    remote_call::detail::toDynamicInternal(dv, v);
    remote_call::detail::fromDynamicInternal(dv, v);
    remote_call::reflection::serializeImpl(dv, v, priority::Hightest);
    remote_call::reflection::deserializeImpl(dv, v, priority::Hightest);
    remote_call::reflection::serialize<decltype(dv)>(v);
    remote_call::reflection::deserialize<decltype(v)>(dv);
    DynamicValue{v};
    v = dv = v;
    v = dv["key"] = v;
    v = dv[4] = v;
    remote_call::exportAs("", "", [](Actor&) {});
    remote_call::importAs<void(Player&)>("", "");
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
    if (t.has_value()) return detail::toDynamicInternal(dv, std::forward<T>(t).as_ptr());
    else dv.emplace<ElementType>(NullValue);
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
        ll::Expected<>         res = detail::fromDynamicInternal(dv, val);
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
