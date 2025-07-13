import "./utils/utils.ts";
import "./example.ts";

import { describe } from "node:test";
import testSimple from "./tests/simple-test.ts";
import testExtra from "./tests/extra-test.ts";

mc.listen("onServerStarted", async () => {
  const timeout = 10 * 1000;
  await new Promise((resolve) => setTimeout(resolve, 1000));
  /// FIXME:
  const timer = setInterval(() => {}, timeout * 10);
  describe("remote-call", { timeout }, () => {
    testSimple();
    testExtra();
  }).finally(() => clearInterval(timer));
});

// success or hasValue?
interface ExpectedValue<T> {
  success: true;
  value: T;
}
interface Unexpected<U = string> {
  success: false;
  error: U;
}
type Expected<T, U = string> = ExpectedValue<T> | Unexpected<U>;
function getValue<T, U>(expected: Expected<T, U>): T {
  if (expected.success) {
    return expected.value;
  }
  if (typeof expected.error == "string") {
    throw new Error(expected.error);
  }
  throw expected.error;
}
