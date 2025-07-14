import assert from "node:assert/strict";
import { it } from "node:test";
import { TEST_EXPORT_NAMESPACE } from "../utils/utils.ts";
import { inspect } from "node:util";

declare global {
  interface RemoteCallTest {
    forwardPartialType(
      param: Partial<Parameters> | { partial: Partial<Parameters["partial"]> }
    ): Parameters | undefined;
  }
}

interface Parameters {
  name: string | "sp";
  spawnPos: IntPos | FloatPos;
  spawnPosDelta: IntPos | FloatPos;
  spawnRotation: [number, number];
  spawnLoadedFromSave: boolean | false;
  xuid: string;
  partial: { a: string; b: string };
  required: {
    pos: [number, number, number];
    dim: number;
  };
}

namespace RemoteCall {
  export const forwardPartialType = ll.imports(TEST_EXPORT_NAMESPACE, "forwardPartialType");
}

export default () =>
  it("partial-test", () => {
    assert.ok(ll.hasExported(TEST_EXPORT_NAMESPACE, "forwardPartialType"));
    const def = RemoteCall.forwardPartialType({});
    assert.ok(def);
    const {
      name,
      spawnPos,
      spawnPosDelta,
      spawnRotation,
      spawnLoadedFromSave,
      xuid,
      partial: { a, b },
      required: { dim, pos },
    } = def;
    // logger.info(inspect(def));
    assert.deepEqual(def, RemoteCall.forwardPartialType({}));
    assert.deepEqual(def, RemoteCall.forwardPartialType(def));
    assert.deepEqual(def, RemoteCall.forwardPartialType({ name, xuid }));
    assert.deepEqual(
      def,
      RemoteCall.forwardPartialType({
        name,
        spawnPos,
        spawnPosDelta,
        spawnRotation,
        spawnLoadedFromSave,
        xuid,
        partial: { a, b },
        required: { dim, pos },
      })
    );
    RemoteCall.forwardPartialType({ partial: { a: "" } });
    // Compile error:
    // RemoteCall.forwardPartialType({ required: { dim: 3 } });
  });
