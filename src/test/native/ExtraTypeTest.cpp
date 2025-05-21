
#include "Test.h"
#include "fmt/core.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Containers.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/reflection/Reflection.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/utility/MCRESULT.h"
#include "mc/scripting/commands/ScriptCommandOrigin.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/server/commands/CommandContext.h"
#include "mc/server/commands/CommandVersion.h"
#include "mc/server/commands/MinecraftCommands.h"
#include "mc/world/Minecraft.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/SaveContextFactory.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "remote_call/api/RemoteCall.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/value/DynamicValue.h"
#include "remote_call/api/value/Values.h"


// Custom Conversion Test(by adl)
template <typename T>
    requires(std::same_as<std::decay_t<T>, DimensionType>)
ll::Expected<> toValue(remote_call::DynamicValue& v, T&& t, remote_call::priority::DefaultTag) {
    return remote_call::toValue(v, t.id, remote_call::priority::Hightest);
}
template <typename T>
    requires(std::same_as<std::decay_t<T>, DimensionType>)
ll::Expected<> fromValue(remote_call::DynamicValue& v, T& t, remote_call::priority::DefaultTag) {
    return remote_call::fromValue(v, t.id, remote_call::priority::Hightest);
}

namespace remote_call::test {


struct BlockInfoJson {
    std::string                block{};
    std::string                extra{};
    std::tuple<int, int, int>  pos;
    DimensionType              dim;
    std::optional<std::string> ba;

    bool operator==(BlockInfoJson const&) const = default;
};
/// TODO:
struct ExtraJson {
    Vec3                                pos;
    BlockPos                            bpos;
    std::pair<Vec3, int>                wpos;
    std::tuple<BlockPos, DimensionType> wbpos;

    Actor* actor{};
    // Actor&        actorRef;
    // Mob*          mob;
    Player*       player{};
    ServerPlayer* sp{};

    Block const* block{};
    // Block*       blockp{};
    BlockActor*                ba{};
    ItemStack*                 item{};
    std::unique_ptr<ItemStack> itemp{};
    Container*                 ctn{};

    BlockInfoJson blockInfo;

    CompoundTag*                 tag{};
    std::unique_ptr<CompoundTag> tagp{};
};


struct BlockInfo {
    Block const*               block{};
    Block const*               extra{};
    BlockPos                   pos;
    DimensionType              dim;
    std::optional<BlockActor*> ba;

    bool     operator==(BlockInfo const&) const = default;
    explicit operator BlockInfoJson() const {
        BlockInfoJson json;
        json.block = block->getTypeName();
        json.extra = extra->getTypeName();
        json.pos   = {pos.x, pos.y, pos.z};
        json.dim   = dim;
        if (ba) json.ba = (*ba)->getName();
        return json;
    }
};

struct Extra {
    Vec3                                pos;
    BlockPos                            bpos;
    std::pair<Vec3, int>                wpos;
    std::tuple<BlockPos, DimensionType> wbpos;

    Actor* actor{};
    // optional_ref<Actor> actorRef{};
    // Mob*          mob;
    Player*       player{};
    ServerPlayer* sp{};

    Block const* block{};
    // Block*       blockp{};
    BlockActor*                ba{};
    ItemStack const*           item{};
    std::unique_ptr<ItemStack> itemp{};
    Container*                 ctn{};

    BlockInfo blockInfo;

    CompoundTag*                 tag{};
    std::unique_ptr<CompoundTag> tagp{};

    bool                operator==(Extra const&) const = default;
    [[nodiscard]] Extra clone() const {
        Extra res = {
            pos,
            bpos,
            wpos,
            wbpos,
            actor,
            // actorRef,
            player,
            sp,
            block,
            ba,
            item,
            {},
            ctn,
            blockInfo,
            tag,
            {}
        };
        if (itemp) res.itemp = std::make_unique<ItemStack>(*itemp);
        if (tagp) res.tagp = tagp->clone();
        return res;
    }
    template <typename T = BlockPos>
    static T randomPos(auto&& dice) {
        return T{
            dice(std::uniform_int_distribution<int>()),
            dice(std::uniform_int_distribution<int>()),
            dice(std::uniform_int_distribution<int>())
        };
    }
    static Extra random() {
        Extra                      e{};
        std::random_device         r;
        std::default_random_engine generator(r());
        auto                       dice = [&generator](auto&& distribution) { return distribution(generator); };

        e.pos   = randomPos<Vec3>(dice);
        e.bpos  = randomPos<BlockPos>(dice);
        e.wpos  = {randomPos<Vec3>(dice), dice(std::uniform_int_distribution<int>())};
        e.wbpos = {randomPos<BlockPos>(dice), dice(std::uniform_int_distribution<int>())};

        e.actor = (decltype(e.actor))dice(std::uniform_int_distribution<intptr_t>());
        // e.actorRef = (decltype(e.actor))dice(std::uniform_int_distribution<intptr_t>());
        e.player = (decltype(e.player))dice(std::uniform_int_distribution<intptr_t>());
        e.sp     = (decltype(e.sp))dice(std::uniform_int_distribution<intptr_t>());
        e.block  = (decltype(e.block))dice(std::uniform_int_distribution<intptr_t>());
        e.ba     = (decltype(e.ba))dice(std::uniform_int_distribution<intptr_t>());
        e.item   = (decltype(e.item))dice(std::uniform_int_distribution<intptr_t>());
        e.ctn    = (decltype(e.ctn))dice(std::uniform_int_distribution<intptr_t>());
        e.tag    = (decltype(e.tag))dice(std::uniform_int_distribution<intptr_t>());
        return e;
    }
};

struct ExtraEx {
    Extra                            extra;
    std::array<Extra, 10>            array;
    ll::DenseMap<std::string, Extra> map;

    bool                  operator==(ExtraEx const&) const = default;
    [[nodiscard]] ExtraEx clone() const {
        ExtraEx ex;
        ex.extra = extra.clone();
        for (auto i : std::views::iota(0, 10)) ex.array[i] = array[i].clone();
        for (auto& [k, v] : map) ex.map[k] = v.clone();
        return ex;
    }
    static ExtraEx random() {
        ExtraEx ex;
        ex.extra = Extra::random();
        for (auto i : std::views::iota(0, 10)) ex.array[i] = Extra::random();
        auto mapview = std::views::iota(0, 15) | std::views::transform([](auto&& i) {
                           return std::pair{std::string("key") + std::to_string(i), Extra::random()};
                       });
        ex.map.insert(mapview.begin(), mapview.end());
        return ex;
    }
};

static_assert(ll::reflection::Reflectable<Extra>);
static_assert(ll::reflection::Reflectable<ExtraEx>);

constexpr auto& ns = TEST_EXPORT_NAMESPACE;
bool            testRandomExtraType() {
    // random test
    auto const random = ExtraEx::random();
    auto       value  = remote_call::reflection::serialize<remote_call::DynamicValue>(random.clone());
    if (!value) throw std::runtime_error(value.error().message());
    auto result = remote_call::reflection::deserialize<ExtraEx>(*value);
    if (!result) throw std::runtime_error(result.error().message());
    assert(random == *result);
    return true;
}

inline bool executeCommand(SimulatedPlayer& sp, std::string const& command) {
    std::string commandOutput;
    auto        context = CommandContext(
        command,
        std::make_unique<ScriptCommandOrigin>(
            sp.getLevel().asServer(),
            &sp.getDimension(),
            [&](int, ::std::string&& output) { commandOutput = output; },
            CommandPermissionLevel::Owner
        ),
        CommandVersion::CurrentVersion()
    );
    auto result = ll::service::getMinecraft()->mCommands->executeCommand(context, false);
    if (!result.mSuccess) getLogger().error("Failed to execute command '{}', output: {}", command, commandOutput);
    return result.mSuccess;
}

ll::coro::CoroTask<bool> testExtraType() {
    assert(testRandomExtraType());
    auto sp = SimulatedPlayer::create(ns, {0, 77, 0});
    if (!sp) throw std::runtime_error("Failed to create simulated player");
    co_await ll::chrono::ticks{40};
    while (!sp->mLocalPlayerInitialized) {
        co_await ll::chrono::ticks{5};
    }
    auto& bs    = sp->getDimensionBlockSource();
    auto  block = Block::tryGetFromRegistry("minecraft:chest");
    auto  bpos  = sp->getFeetBlockPos() - BlockPos{0, 1, 0};
    bs.setBlock(bpos, *block, 3, nullptr, nullptr);
    co_await ll::chrono::ticks{1};
    assert(executeCommand(*sp, "give @a wood"));

    Extra extra;
    extra.pos   = sp->getPosition();
    extra.bpos  = sp->getFeetBlockPos();
    extra.wpos  = {sp->getHeadPos(), sp->getDimensionId() + 1};
    extra.wbpos = {bpos, sp->getDimensionId() + 2};

    extra.actor = sp;
    // extra.actorRef  = sp;
    extra.player = sp;
    extra.sp     = sp;
    extra.block  = &bs.getBlock(bpos);
    extra.ba     = bs.getBlockEntity(bpos);
    assert(extra.ba);
    extra.itemp     = std::make_unique<ItemStack>("minecraft:stone");
    extra.item      = extra.itemp.get();
    extra.ctn       = sp->getEnderChestContainer();
    extra.blockInfo = {extra.block, extra.block, extra.pos, DimensionType{0}, {}};
    auto ctx        = SaveContextFactory::createCloneSaveContext();
    extra.tagp      = extra.itemp->save(*ctx);
    extra.tag       = extra.tagp.get();

    auto deadline = ll::chrono::GameTickClock::now() + ll::chrono::ticks{100};
    while (!remote_call::hasFunc(ns, "lseTestExtra")) {
        assert(ll::chrono::GameTickClock::now() <= deadline);
        co_await ll::chrono::ticks{5};
    }

    auto const lseTestExtra = remote_call::importAs<Extra(Extra&&)>(ns, "lseTestExtra");
    auto       ret          = lseTestExtra(extra.clone());
    assert(ret);
    // assert(extra == ret); // CompoundTag* -> std::unique_ptr<CompoundTag>?
    co_return true;
};

auto extraTestStarted =
    (testExtraType().launch(
         ll::thread::ServerThreadExecutor::getDefault(),
         [](ll::Expected<bool>&& result) {
             if (result) test::success("ExtraType Test passed");
             else result.error().log(getLogger());
         }
     ),
     true);
} // namespace remote_call::test
