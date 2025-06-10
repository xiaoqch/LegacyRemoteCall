logger.info("Start Example Test");
class RecipesApi {
    static addShapeRecipe = ll.imports("RecipesApi", "addShapeRecipe");
}
setTimeout(() => {
    logger.info("Registering recipe custom:lse_recipe");
    const shape = [
        "A A", //
        " A ",
        "A A",
    ];
    const types = [{ id: "minecraft:stone", flag: "A" }];
    const result = RecipesApi.addShapeRecipe("custom:lse_recipe", mc.newItem("minecraft:gold_block", 3), shape, types, ["crafting_table"], 2);
    if (result)
        logger.info("Recipe custom:lse_recipe added");
    else
        logger.error("Failed to add recipe custom:lse_recipe");
}, 50);
export {};
