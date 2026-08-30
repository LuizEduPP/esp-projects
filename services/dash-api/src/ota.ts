import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";

import { config } from "./config.js";

type Manifest = { version: string; sha256: string; size: number };

const DIR = resolve(process.env.DASH_DATA_DIR || "data", "firmware");
const BIN = resolve(DIR, "firmware.bin");
const META = resolve(DIR, "manifest.json");

export const otaManifest = (): Manifest | null => {
  try {
    return JSON.parse(readFileSync(META, "utf8")) as Manifest;
  } catch {
    return null;
  }
};

export const otaBinary = (): Buffer | null => (existsSync(BIN) ? readFileSync(BIN) : null);

export const otaPublish = (version: string, binary: Buffer): Manifest => {
  mkdirSync(DIR, { recursive: true });
  writeFileSync(BIN, binary);

  const manifest: Manifest = {
    version,
    sha256: createHash("sha256").update(binary).digest("hex"),
    size: binary.length,
  };
  writeFileSync(META, JSON.stringify(manifest, null, 2));

  console.log(`[ota] publicado ${version}, ${binary.length} bytes`);
  return manifest;
};

export const otaAuthorized = (header: string | undefined): boolean => {
  if (!config.otaToken) return true;
  return header === config.otaToken;
};
