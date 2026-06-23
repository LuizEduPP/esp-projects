/**
 * Load ~/.folio/config.json (or FOLIO_CONFIG path). Env vars override file values.
 */
import { existsSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

function configPaths() {
  const paths = [];
  if (process.env.FOLIO_CONFIG) {
    paths.push(process.env.FOLIO_CONFIG);
  }
  paths.push(join(homedir(), ".folio", "config.json"));
  return paths;
}

export function loadFileConfig() {
  for (const path of configPaths()) {
    if (existsSync(path)) {
      const raw = readFileSync(path, "utf8");
      return { path, data: JSON.parse(raw) };
    }
  }
  return { path: null, data: {} };
}

function getPath(obj, dotPath) {
  return dotPath.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

export function makeResolver(fileData) {
  const num = (fileKey, envKey, fallback) => {
    if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
      return Number(process.env[envKey]);
    }
    const v = getPath(fileData, fileKey);
    return v !== undefined ? Number(v) : fallback;
  };
  const str = (fileKey, envKey, fallback) => {
    if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
      return process.env[envKey];
    }
    const v = getPath(fileData, fileKey);
    return v !== undefined ? String(v) : fallback;
  };
  const bool = (fileKey, envKey, fallback) => {
    if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
      return process.env[envKey] !== "0" && process.env[envKey] !== "false";
    }
    const v = getPath(fileData, fileKey);
    return v !== undefined ? Boolean(v) : fallback;
  };
  return { num, str, bool };
}
