import { join } from "node:path";
import {
  editableConfig,
  getConfigPath,
  nodeConfigPayload,
  nodeConfigVersion,
  saveConfigPatch,
} from "./config-store.mjs";
import { applyCfgTo, buildCfgFromFile } from "./config-runtime.mjs";

export const CFG = buildCfgFromFile();

export function reloadConfig() {
  applyCfgTo(CFG);
  return editableConfig();
}

export const PATHS = {
  db: () => join(CFG.dataDir, "folio.db"),
  audioDir: (day) => join(CFG.dataDir, "audio", day),
  frameDir: (day) => join(CFG.dataDir, "frames", day),
  speakerDir: () => join(CFG.dataDir, "speakers"),
  digestDir: () => join(CFG.dataDir, "digests"),
};

export function publicConfig() {
  const full = editableConfig();
  return {
    ...full,
    configPath: getConfigPath(),
    node: nodeConfigPayload(),
  };
}

export function updateConfig(patch) {
  const result = saveConfigPatch(patch);
  reloadConfig();
  return { ...result, config: editableConfig() };
}

export { nodeConfigPayload, nodeConfigVersion };
