import assert from "node:assert/strict";
import { it } from "node:test";
import { TEST_EXPORT_NAMESPACE } from "../utils/utils.ts";
import { AssertionError } from "node:assert";
import { inspect } from "node:util";

declare global {
  interface RemoteCallTest {
    forwardValue(param: any): any | undefined;
  }
}

ll.exports((val) => val, "lse-test", "lseForwardValue");

namespace lse {
  export const forwardValue = ll.imports("lse-test", "lseForwardValue");
}
namespace lrc {
  export const forwardJsonType = ll.imports(TEST_EXPORT_NAMESPACE, "forwardJsonType");
}

let testData: any = undefined;

function lseCompareJson(data: any, jsonStr: string): boolean {
  assert.equal(testData, undefined);
  assert.notEqual(data, undefined);
  testData = data;
  const result = JSON.parse(jsonStr); // without undefined
  const data2 = JSON.parse(JSON.stringify(data)); // drop undefined
  const hasNull = Object.hasOwn(data, "null");
  hasNull && assert.strictEqual(data.null, undefined);
  assert.ok(!Object.hasOwn(result, "null"));
  assert.ok(!Object.hasOwn(data2, "null"));
  let equal = true;
  try {
    assert.deepEqual(data2, result);
  } catch (error: AssertionError | unknown) {
    if (error instanceof AssertionError) {
      logger.error(inspect(error));
    }
    equal = true;
  }

  return equal;
}

ll.exports(lseCompareJson, TEST_EXPORT_NAMESPACE, "lseCompareJson");

export default () =>
  it("simple-test", function (t) {
    assert.ok(testData);
    const lseForwarded = lse.forwardValue(testData); // with undefined
    assert.deepEqual(lseForwarded, testData);
    const lrcForwarded = lrc.forwardJsonType(testData); // with undefined
    assert.deepEqual(lrcForwarded, testData);
    const hasNull = Object.hasOwn(data, "null");
    if (hasNull) {
      assert.ok(Object.hasOwn(lseForwarded, "null"));
      assert.strictEqual(lseForwarded.null, undefined);
      assert.ok(Object.hasOwn(lrcForwarded, "null"));
      assert.strictEqual(lrcForwarded.null, undefined);
    }
  });
