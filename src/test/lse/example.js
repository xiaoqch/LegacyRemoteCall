class RecipeApi {
    static addShapeRecipe = ll.imports("RecipesReg", "addShapeRecipe");
}
mc.listen("onServerStarted", () => {
    const shape = [
        "A A", //
        " A ",
        "A A",
    ];
    const types = [{ id: "minecraft:stone", tag: "A" }];
    const result = RecipeApi.addShapeRecipe("lse_recipe", mc.newItem("minecraft:gold", 3), shape, types, ["unknown"], 2, true);
    if (result)
        logger.info("Success");
    else
        logger.error("Error");
});
export {};
