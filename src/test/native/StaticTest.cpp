#include "remote_call/api/RemoteCall.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/conversion/DefaultConversion.h"
#include "remote_call/api/reflection/Serialization.h"
#include "remote_call/api/reflection/SerializationExt.h"
#include "remote_call/api/value/DynamicValue.h"

namespace remote_call::test {

using remote_call::concepts::SupportFromValue;
using remote_call::concepts::SupportToValue;

template <typename T>
concept FullSupported = SupportToValue<T> && SupportFromValue<T>;

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
static_assert(FullSupported<CompoundTag*>);

static_assert(requires(DynamicValue j, std::tuple<Vec3, DimensionType> v) {
    remote_call::serializeImpl(j, v, priority::Hightest);
    remote_call::deserializeImpl(j, v, priority::Hightest);
    remote_call::toValue(j, v, ll::meta::PriorityTag<10>{});
    remote_call::fromValue(j, v, priority::Hightest);
    remote_call::detail::toValueInternal(j, v);
    remote_call::detail::fromValueInternal(j, v);
    remote_call::reflection::serializeImpl(j, v, priority::Hightest);
    remote_call::reflection::deserializeImpl(j, v, priority::Hightest);
    remote_call::reflection::serialize<decltype(j)>(v);
    remote_call::reflection::deserialize<decltype(v)>(j);
    DynamicValue{v};
    v = j = v;
    v = j["key"] = v;
    v = j[4] = v;
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
    requires(concepts::SupportFromValue<decltype(std::declval<T>().as_ptr())>)
ll::Expected<> toValue1(DynamicValue& v, T&& t, priority::LowTag) {
    if (t.has_value()) return detail::toValueInternal(v, std::forward<T>(t).as_ptr());
    else v.emplace<ElementType>(nullptr);
    return {};
}
template <IsOptionalRef T>
    requires(concepts::SupportFromValue<decltype(std::declval<T>().as_ptr())>)
ll::Expected<> fromValue1(DynamicValue& v, T& t, priority::DefaultTag) {
    if (v.hold<std::nullptr_t>()) {
        t = {};
        return {};
    } else {
        typename T::value_type val{};
        ll::Expected<>         res = detail::fromValueInternal(v, val);
        if (res) t = val;
        return res;
    }
}
static_assert(requires(DynamicValue j, optional_ref<Actor>& v) {
    toValue1(j, std::forward<decltype(v)>(v), priority::Hightest);
    fromValue1(j, std::forward<decltype(v)>(v), priority::Hightest);
});

static_assert(!concepts::IsString<nullptr_t>);
static_assert(concepts::IsTupleLike<std::tuple<>>);
static_assert(requires(std::string& str, remote_call::DynamicValue& j) {
    str = std::string{std::forward<remote_call::DynamicValue>(j)};
});

} // namespace remote_call::test
