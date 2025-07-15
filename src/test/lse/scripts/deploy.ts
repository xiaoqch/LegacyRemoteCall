import fs from "fs/promises";
import path from "path";
import packageJson from "../package.json" with { type: "json" };
import manifestJson from "../manifest.json" with { type: "json" };
import { parseArgs } from "util";

const args = parseArgs({
  options: {
    copy: {
      type: "boolean",
      default: false,
      short: "c",
      multiple: false,
    },
  },
});

const modName = manifestJson.name;
const entryPath = manifestJson.entry;
const version = manifestJson.version ?? packageJson.version ?? "0.0.1";
const description = manifestJson.description ?? packageJson.description;
const author = manifestJson.author ?? packageJson.author;

const packageDir = path.join(import.meta.dirname, "..");
const projectDir = path.join(packageDir, "..", "..", "..");
const targetPath = path.join(projectDir, "bin", modName);

async function generateProxyMod() {
  await fs.mkdir(path.dirname(path.resolve(targetPath, entryPath)), { recursive: true });

  const proxyPackageJson = {
    name: `${modName}-proxy`,
    version,
    description,
    author,
    type: packageJson.type,
    main: entryPath,
    scripts: {},
    dependencies: {
      [packageJson.name]: `file:${path.resolve(packageDir)}`,
    },
  } as const;

  const proxyManifestJson = {
    entry: entryPath,
    name: modName,
    type: "lse-nodejs",
    version,
    description,
    dependencies: [
      {
        name: "legacy-script-engine-nodejs",
      },
    ],
    extraInfo: {
      author,
    },
  } as const;

  const proxyEntry = `import "${packageJson.name}";`;
  await Promise.all([
    fs.writeFile(
      path.join(targetPath, "package.json"),
      JSON.stringify(proxyPackageJson, undefined, 4)
    ),
    fs.writeFile(
      path.join(targetPath, "manifest.json"),
      JSON.stringify(proxyManifestJson, undefined, 4)
    ),
    fs.writeFile(path.join(targetPath, proxyManifestJson.entry), proxyEntry),
  ]);
  console.info(`Proxy mod "${proxyManifestJson.name}" has been generated in "${targetPath}`);
}

async function copyMod() {
  const { promise, resolve, reject } =
    Promise.withResolvers<[{ files: { path: string; size: number; mode: number }[] }]>();
  let inputData = "";
  process.stdin.on("data", (chunk) => {
    inputData += chunk.toString();
  });
  process.stdin.on("end", () => {
    resolve(JSON.parse(inputData));
  });
  const timer = setTimeout(
    () => reject(new Error(`Please pass "npm pack --json" output to script by pipeline.`)),
    100
  );
  const input = await promise.finally(() => clearTimeout(timer));

  const files = input[0].files.map(({ path }) => path);
  const cp = (name: string) =>
    fs.cp(path.resolve(packageDir, name), path.resolve(targetPath, name));
  await Promise.all(files.map(cp));
  console.info(`Test mod "${manifestJson.name}" has been copied to "${targetPath}`);
}

if (args.values.copy) {
  await copyMod();
} else {
  await generateProxyMod();
}
