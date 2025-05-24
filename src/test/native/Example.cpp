#include "ll/api/Expected.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/server/ServerStartedEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/core/sem_ver/SemVersion.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/network/packet/CraftingDataPacket.h"
#include "mc/safety/RedactableString.h"
#include "mc/server/commands/MinecraftCommands.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/NetworkItemInstanceDescriptor.h"
#include "mc/world/item/crafting/Recipe.h"
#include "mc/world/item/crafting/Recipes.h"
#include "mc/world/item/crafting/ShapedRecipe.h"
#include "mc/world/item/crafting/ShapelessRecipe.h"
#include "mc/world/item/registry/ItemRegistry.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockLegacy.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "remote_call/api/RemoteCall.h"
#include "remote_call/api/base/Meta.h"
#include "remote_call/api/value/DynamicValue.h"
#include "test/native/Test.h"

// Custom conversions
inline ll::Expected<>
fromDynamicImpl(remote_call::DynamicValue& dv, Bedrock::Safety::RedactableString& rs, remote_call::priority::HightTag) {
    std::string str{};
    auto        res = remote_call::fromDynamic(dv, str);
    if (res) rs.append(str);
    return res;
}
inline ll::Expected<>
fromDynamicImpl(remote_call::DynamicValue& dv, ItemInstance& ii, remote_call::priority::HightTag) {
    ItemStack* is;
    auto       res = remote_call::fromDynamic(dv, is);
    if (res) ii = ItemInstance(*is);
    return res;
}

inline ll::Expected<>
fromDynamicImpl(remote_call::DynamicValue& dv, HashedString& hs, remote_call::priority::HightTag) {
    std::string str{};
    auto        res = remote_call::fromDynamic(dv, str);
    if (res) void(hs = HashedString(std::move(str)));
    return res;
}
struct MyType {
    std::string id;
    std::string flag;
    explicit    operator Recipes::Type() const {
        /// TODO: not implemented
        auto  item       = ll::service::getLevel()->getItemRegistry().lookupByName("minecraft:stone");
        auto& block      = BlockTypeRegistry::lookupByName("minecraft:stone")->mDefaultState;
        auto  ingredient = RecipeIngredient("minecraft:stone", 1, 0);
        return {item.get(), block, ingredient, flag.at(0)};
    }
};
// TODO: without default constructor
template <std::same_as<Recipes::Type> Ret>
inline ll::Expected<Ret> fromDynamicImpl(remote_call::DynamicValue& dv, remote_call::priority::HightTag) {
    MyType type{};
    auto   res = remote_call::fromDynamic(dv, type);
    if (res) return static_cast<Recipes::Type>(type);
    return res;
}

// Function
bool addShapeRecipe(
    ::std::string const&                 recipeId,
    ::ItemInstance const&                result,
    ::std::vector<::std::string> const&  rows,
    ::std::vector<MyType> const&         myTypes,
    ::std::vector<::HashedString> const& tags,
    int                                  priority,
    // ::RecipeUnlockingRequirement const&   unlockingReq,
    // ::SemVersion const&                   formatVersion,
    bool assumeSymmetry
) {
    auto& recipes = ll::service::getLevel()->getRecipes();
    auto  view    = myTypes | std::views::transform([](auto&& t) { return static_cast<Recipes::Type>(t); });
    try {
        recipes.addShapedRecipe(
            recipeId,
            result,
            rows,
            std::vector<Recipes::Type>(view.begin(), view.end()),
            tags,
            priority,
            nullptr,
            RecipeUnlockingRequirement(),
            SemVersion(),
            assumeSymmetry
        );
        return true;
    } catch (...) {
        ll::makeExceptionError().error().log(remote_call::test::getLogger());
        /// TODO: return ll::Expected<>;
        // return ll::makeExceptionError();
    }
    return false;
}

ll::Expected<> exportApi() {
    ll::Expected<> res;
    if (res) res = remote_call::exportAs("RecipesReg", "addShapeRecipe", addShapeRecipe);
    if (!res) res.error().log(remote_call::test::getLogger());
    return res;
}

auto started = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerStartedEvent>([](auto&&) {
    (void)exportApi();
});