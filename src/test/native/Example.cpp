#include "test/native/Test.h"

/** Example */

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

// Custom conversions

// element type
inline ll::Expected<>
fromDynamic(remote_call::DynamicValue& dv, Bedrock::Safety::RedactableString& rs, remote_call::priority::HightTag) {
    if (!dv.hold<std::string>())
        return remote_call::error_utils::makeFromDynamicTypeError<Bedrock::Safety::RedactableString, std::string>(dv);
    rs.append(dv.get<std::string>());
    return {};
}

// tryGet
inline ll::Expected<> fromDynamic(remote_call::DynamicValue& dv, ItemInstance& ii, remote_call::priority::HightTag) {
    return dv.tryGet<ItemStack*>().transform([&](auto&& is) { ii = ItemInstance(*is); });
}

// getTo
inline ll::Expected<> fromDynamic(remote_call::DynamicValue& dv, HashedString& hs, remote_call::priority::HightTag) {
    hs.clear();
    return dv.getTo(hs.mStr).transform([&]() { hs.mStrHash = HashedString::computeHash(hs.mStr); });
}
struct TempRecipesType {
    HashedString id;
    std::string  flag;

    explicit operator Recipes::Type() && {
        auto  item       = ll::service::getLevel()->getItemRegistry().lookupByName(id);
        auto& block      = BlockTypeRegistry::lookupByName(id)->mDefaultState;
        auto  ingredient = RecipeIngredient(id, 1, 0);
        return {item.get(), block, ingredient, flag.at(0)};
    }
};

// Without default constructor
inline ll::Expected<Recipes::Type>
fromDynamic(remote_call::DynamicValue& dv, std::in_place_type_t<Recipes::Type>, remote_call::priority::HightTag) {
    return dv.tryGet<TempRecipesType>().transform([](TempRecipesType&& type) {
        return static_cast<Recipes::Type>(std::move(type));
    });
}

// Function
ll::Expected<bool> addShapeRecipe(
    ::std::string const&                 recipeId,
    ::ItemInstance const&                result,
    ::std::vector<::std::string> const&  rows,
    ::std::vector<Recipes::Type> const&  types,
    ::std::vector<::HashedString> const& tags,
    int                                  priority = 2,
    // ::RecipeUnlockingRequirement const&   unlockingReq,
    // ::SemVersion const&                   formatVersion,
    bool assumeSymmetry = true
) {
    // auto& recipes2 = ll::service::getLevel()->getRecipes();
    auto&                                   recipes = ll::service::getLevel()->getRecipes();
    ::ll::TypedStorage<8, 24, ::SemVersion> ver{};
    RecipeUnlockingRequirement              req;
    try {
        recipes.addShapedRecipe(recipeId, result, rows, types, tags, priority, {}, req, ver, assumeSymmetry);

        HashedString key{recipeId};
        auto const&  map = *recipes.mRecipes;
        for (auto& tag : tags) {
            if (!map.contains(tag) || !map.at(tag).contains(key)) {
                return ll::makeStringError(fmt::format(
                    "Unknown Error occurred. Recipe with id '{}' and tag '{}' not be found",
                    recipeId,
                    tag.getString()
                ));
            }
        }
        if (ll::service::getLevel()->getPlayerList().size() > 0)
            CraftingDataPacket::prepareFromRecipes(recipes, true)->sendToClients();
        return true;
    } catch (...) {
        return ll::makeExceptionError();
    }
}

ll::Expected<> exportApi() {
    ll::Expected<> res;
    if (res) res = REMOTE_CALL_EXPORT_EX("RecipesApi", "addShapeRecipe", addShapeRecipe);
    return res;
}

auto listener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerStartedEvent>([](auto&&) {
    exportApi().or_else([](auto&& error) {
        error.log(remote_call::test::getLogger());
        return ll::Expected<>{};
    });
});
