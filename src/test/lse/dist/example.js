class RecipeApi {
    static addShapeRecipe = ll.imports("RecipesReg", "addShapeRecipe");
}
mc.listen("onServerStarted", () => {
    const shape = [
        "A A", //
        " A ",
        "A A",
    ];
    const types = [{ id: "minecraft:stone", flag: "A" }];
    const result = RecipeApi.addShapeRecipe("custom:lse_recipe", mc.newItem("minecraft:gold_block", 3), shape, types, ["crafting_table"], 2, true);
    if (result)
        logger.info("Success");
    else
        logger.error("Error");
});
export {};
