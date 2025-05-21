import { mkdirSync, writeFileSync } from "fs";

const TEST_EXPORT_NAMESPACE = "RemoteCallTest";

logger.info("Start RemoteCall Test for LegacyScriptEngine");

function deepEqual(obj1: any, obj2: any): boolean {
  // Check if both are the same reference
  if (obj1 === obj2) {
    return true;
  }
  if ((obj1 ?? undefined) === undefined && (obj2 ?? undefined) === undefined)
    return true;
  if (
    typeof obj1 !== "object" ||
    typeof obj2 !== "object" ||
    (obj1 ?? undefined) === undefined ||
    (obj2 ?? undefined) === undefined
  ) {
    // Check if both are objects and not null
    return false;
  }

  // Compare keys
  const keys1 = Object.keys(obj1);
  const keys2 = Object.keys(obj2);

  if (keys1.length !== keys2.length) {
    return false;
  }

  // Recursively compare values
  for (const key of keys1) {
    if (!keys2.includes(key) || !deepEqual(obj1[key], obj2[key])) {
      return false;
    }
  }

  return true;
}

ll.exports((val) => val, "lse-test", "lseForwardValue");
const lseForwardValue = ll.imports("lse-test", "lseForwardValue");

const dataDir = "plugins/lse-test/data/";
mkdirSync(dataDir, { recursive: true });
ll.exports(
  (data: {}, jsonStr: string) => {
    try {
      const result = JSON.parse(jsonStr) as {};
      if (deepEqual(data, result)) {
        const forwardJsonType = ll.imports(
          TEST_EXPORT_NAMESPACE,
          "forwardJsonType"
        ) as ({}) => {};
        const forwarded1 = lseForwardValue(result);
        if (!deepEqual(result, forwarded1)) {
          writeFileSync(`${dataDir}/result.json`, JSON.stringify(result));
          writeFileSync(
            `${dataDir}/forwarded1.json`,
            JSON.stringify(forwarded1)
          );
          logger.error(`lseForwardValue changes the data`);
          // return false
        }
        const forwarded = forwardJsonType(result);
        if (deepEqual(result, forwarded)) {
          colorLog("green", "RemoteCall Test for LegacyScriptEngine Passed");
          return true;
        }
        logger.error("forwardJsonType changes the data");
        writeFileSync(`${dataDir}/result.json`, JSON.stringify(result));
        writeFileSync(`${dataDir}/forwarded.json`, JSON.stringify(forwarded));
        logger.error(JSON.stringify(forwarded).substring(0, 500));
        logger.error(JSON.stringify(result).substring(0, 500));
        return false;
      }
      logger.error("testJsonType data not equals");
      writeFileSync(`${dataDir}/data.json`, JSON.stringify(data));
      writeFileSync(`${dataDir}/result.json`, JSON.stringify(result));
      logger.error(JSON.stringify(data).substring(0, 500));
      logger.error(JSON.stringify(result).substring(0, 500));
      return false;
    } catch (error: Error | any) {
      import("util")
        .then((util) => {
          logger.error(util.inspect(error));
        })
        .catch(() => {
          logger.error(error?.stack ?? error ?? "Unknown Error");
        });
    }
    return false;
  },
  TEST_EXPORT_NAMESPACE,
  "lseCompareJson"
);

ll.exports(
  (extra: {}) => {
    if (extra === undefined) {
      logger.error("Received undefined");
      return extra;
    }
    const replacer = (key: string, val: any) => {
      if (typeof val == "object") {
        if (val instanceof LLSE_Item) {
          return `<Item ${val.name}>`;
        } else if (val instanceof LLSE_Block) {
          return `<Block ${val.name}>`;
        } else if (val instanceof LLSE_BlockEntity) {
          return `<BlockEntity ${val.name}>`;
        } else if (val instanceof LLSE_Container) {
          return `<Container ${val.type}>`;
        } else if (val instanceof LLSE_CustomForm) {
          return `<CustomForm ${val}>`;
        } else if (val instanceof LLSE_Device) {
          return `<Device ${val}>`;
        } else if (val instanceof LLSE_Entity) {
          return `<Entity ${val.name}>`;
        } else if (val instanceof LLSE_Objective) {
          return `<Objective ${val.name}>`;
        } else if (val instanceof LLSE_Player) {
          return `<Player ${val.name}>`;
        } else if (val instanceof LLSE_SimpleForm) {
          return `<SimpleForm ${val}>`;
        } else if (val instanceof IntPos) {
          return `<Block ${val.toString()}>`;
        } else if (val instanceof FloatPos) {
          return `<Block ${val.toString()}>`;
          // } else if (val instanceof NbtEnd) {
          //   return `<NbtEnd ${val.toString()}>`
        } else if (val instanceof NbtByte) {
          return `<NbtByte ${val.toString()}>`;
        } else if (val instanceof NbtShort) {
          return `<NbtShort ${val.toString()}>`;
        } else if (val instanceof NbtInt) {
          return `<NbtInt ${val.toString()}>`;
        } else if (val instanceof NbtLong) {
          return `<NbtLong ${val.toString()}>`;
        } else if (val instanceof NbtFloat) {
          return `<NbtFloat ${val.toString()}>`;
        } else if (val instanceof NbtDouble) {
          return `<NbtDouble ${val.toString()}>`;
        } else if (val instanceof NbtByteArray) {
          return `<NbtByteArray ${val.toString()}>`;
        } else if (val instanceof NbtString) {
          return `<NbtString ${val.toString()}>`;
        } else if (val instanceof NbtList) {
          return `<NbtList ${val.getSize()} entries>`;
        } else if (val instanceof NbtCompound) {
          return `<NbtCompound ${val.getKeys().length} entries>`;
        }
      }
      return val;
    };
    logger.info("Received Extra Data: ", JSON.stringify(extra, replacer, 2));
    return extra;
  },
  TEST_EXPORT_NAMESPACE,
  "lseTestExtra"
);
