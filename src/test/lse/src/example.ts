logger.info("Start Example Test");

interface RecipeType {
  id: string;
  flag: string;
}

interface RecipesApi {
  addShapeRecipe(
    recipeId: string,
    result: Item,
    rows: readonly [string, string, string],
    types: readonly RecipeType[],
    tags: readonly string[],
    priority?: number,
    assumeSymmetry?: boolean
  ): boolean | undefined;
}

declare global {
  namespace ll {
    function imports<Name extends keyof RecipesApi>(
      namespace: "RecipesApi",
      name: Name
    ): RecipesApi[Name];
  }
}

class RecipesApi {
  static addShapeRecipe = ll.imports("RecipesApi", "addShapeRecipe");
}
setTimeout(() => {
  logger.info("Registering recipe custom:lse_recipe");
  const shape = [
    "A A", //
    " A ",
    "A A",
  ] as const;
  const types: RecipeType[] = [{ id: "minecraft:stone", flag: "A" }];
  const result = RecipesApi.addShapeRecipe(
    "custom:lse_recipe",
    mc.newItem("minecraft:gold_block", 3) as Item,
    shape,
    types,
    ["crafting_table"],
    2
  );
  if (result) logger.info("Recipe custom:lse_recipe added");
  else logger.error("Failed to add recipe custom:lse_recipe");
}, 50);
