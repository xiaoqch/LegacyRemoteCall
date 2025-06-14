#include "Test.h" // IWYU pragma: keep
#include "ll/api/base/Containers.h"
#include "remote_call/api/API.h" // IWYU pragma: keep


namespace remote_call::test {

using remote_call::concepts::SupportFromDynamic;
using remote_call::concepts::SupportToDynamic;

template <typename T>
concept FullSupported = SupportToDynamic<T> && SupportFromDynamic<T>;

#define TEST_SIMPLE_TYPES                                                                                              \
    uchar, short, int, bool, float, double, size_t, std::string, std::optional<std::string>, std::nullptr_t,           \
        std::monostate
#define TEST_EXTRA_TYPES                                                                                               \
    Vec3, BlockPos, std::pair<BlockPos, int>, std::pair<Vec3, char>, std::pair<Vec3, DimensionType>, Actor*,           \
        std::reference_wrapper<CompoundTag>, Player*, ServerPlayer const*, Block const*, BlockActor*,                  \
        ItemStack const*, std::unique_ptr<ItemStack>, Container*, CompoundTag*, std::unique_ptr<CompoundTag>

static_assert(std::same_as<impl::corrected_arg<int>::hold_type, int>);
static_assert(std::same_as<impl::corrected_arg<std::string&>::hold_type, std::string>);
static_assert(std::same_as<impl::corrected_arg<Player&>::hold_type, std::reference_wrapper<Player>>);

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
static_assert(SupportToDynamic<std::nullopt_t>);
static_assert(FullSupported<std::monostate>);

static_assert(FullSupported<Vec3>);
static_assert(FullSupported<BlockPos>);
static_assert(FullSupported<std::pair<BlockPos, int>>);
static_assert(FullSupported<std::pair<Vec3, char>>);
static_assert(FullSupported<std::pair<Vec3, DimensionType>>);

static_assert(FullSupported<Actor*>);
static_assert(SupportToDynamic<Actor&>);
static_assert(SupportFromDynamic<std::reference_wrapper<Actor>>);
static_assert(FullSupported<std::reference_wrapper<CompoundTag>>);

// static_assert(FullSupported<Mob*>);
static_assert(FullSupported<Player*>);
static_assert(FullSupported<ServerPlayer const*>);
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
static_assert(FullSupported<ll::StringMap<std::unique_ptr<CompoundTag>>>);
static_assert(FullSupported<ll::SmallStringMap<std::unique_ptr<ItemStack>>>);
static_assert(FullSupported<ll::StringNodeMap<Container*>>);
static_assert(FullSupported<ll::SmallStringNodeMap<std::string>>);
static_assert(FullSupported<ll::SmallDenseSet<std::string>>);

static_assert(!concepts::IsString<nullptr_t>);
static_assert(concepts::IsTupleLike<std::tuple<>>);

} // namespace remote_call::test
