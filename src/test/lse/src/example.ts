interface RecipeType {
  id: string;
  flag: string;
}

interface RecipeApi {
  addShapeRecipe(
    recipeId: string,
    result: Item,
    rows: readonly [string, string, string],
    types: readonly RecipeType[],
    tags: readonly string[],
    priority: number,
    assumeSymmetry: boolean
  ): boolean | undefined;
}

declare global {
  namespace ll {
    function imports<Name extends keyof RecipeApi>(
      namespace: "RecipesReg",
      name: Name
    ): RecipeApi[Name];
  }
}

class RecipeApi {
  static addShapeRecipe = ll.imports("RecipesReg", "addShapeRecipe");
}

mc.listen("onServerStarted", () => {
  const shape = [
    "A A", //
    " A ",
    "A A",
  ] as const;
  const types: RecipeType[] = [{ id: "minecraft:stone", flag: "A" }];
  const result = RecipeApi.addShapeRecipe(
    "custom:lse_recipe",
    mc.newItem("minecraft:gold_block", 3) as Item,
    shape,
    types,
    ["crafting_table"],
    2,
    true
  );
  if (result) logger.info("Success");
  else logger.error("Error");
});
