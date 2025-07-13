import assert from "node:assert";
import { it } from "node:test";
import { TEST_EXPORT_NAMESPACE } from "../utils/utils.ts";

const { promise, resolve, reject } = Promise.withResolvers<any>();

function lseTestExtra(extra: {}): {} {
  assert.notEqual(extra, undefined);
  resolve(extra);
  return extra;
}

ll.exports(lseTestExtra, TEST_EXPORT_NAMESPACE, "lseTestExtra");

export default () =>
  it("extra-test", async () => {
    const testData = await promise;
    assert.ok(testData);
    const replacer = (key: string, val: any) => {
      if (typeof val == "undefined") {
        return null;
      } else if (typeof val == "object") {
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
    // logger.info(inspect(testData));
    // logger.info("Received Extra Data: ", JSON.stringify(testData, replacer, 2));
  });
