import fs from "fs/promises";
import path from "path";
import packageJson from "../package.json" with { type: "json" };
import manifestJson from "../manifest.json" with { type: "json" };

const modName = manifestJson.name;
const entryPath = manifestJson.entry;
const version = manifestJson.version ?? packageJson.version ?? "0.0.1";
const description = manifestJson.description ?? packageJson.description;
const author = manifestJson.author ?? packageJson.author;

const packageDir = path.join(import.meta.dirname, "..");
const projectDir = path.join(packageDir, "..", "..", "..");
const targetPath = path.join(projectDir, "bin", modName);

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

console.info(`Proxy mod "${proxyManifestJson.name}" generated in "${targetPath}`);
