import inspector from "inspector";
import path, { sep } from "path";

declare global {
  interface RemoteCallTest {}
  namespace ll {
    function imports<Name extends keyof RemoteCallTest>(
      namespace: "RemoteCallTest",
      name: Name
    ): RemoteCallTest[Name];
  }
}

declare global {
  namespace ll {
    function getCurrentPluginInfo(): Plugin;
  }
}

export const TEST_EXPORT_NAMESPACE = "RemoteCallTest";

// Debugger
let inspectorIns: ReturnType<typeof inspector.open> | undefined = undefined;
export function startInspector(wait: boolean = false) {
  inspectorIns || (ll as any).onUnload(inspector.close);
  inspectorIns ??= inspector.open(undefined, undefined, true);
}
export function debugbreak() {
  startInspector(true);
  inspector.waitForDebugger();
  debugger;
}

// Plugin Info
export const self = ll.getCurrentPluginInfo();
export function getSelfDir() {
  return path.join(ll.pluginsRoot, self.name) + path.sep;
}
export function getDataPath(name?: string) {
  if (name === undefined) {
    return (path.join(getSelfDir(), "data") + path, sep);
  } else {
    return path.join(getSelfDir(), "data", name);
  }
}
export function getConfigPath(name?: string) {
  if (name === undefined) {
    return (path.join(getSelfDir(), "config") + path, sep);
  } else {
    return path.join(getSelfDir(), "config", name);
  }
}
