import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { CFG } from "./config.mjs";
import { getAudioChunk, getFrame } from "./db.mjs";
import { pcmToWav } from "./util.mjs";

function assertUnderDataDir(filePath) {
  const base = resolve(CFG.dataDir);
  const abs = resolve(filePath);
  if (abs !== base && !abs.startsWith(`${base}/`)) {
    throw new Error("path outside data directory");
  }
  return abs;
}

export function serveFrame(db, frameId) {
  const frame = getFrame(db, Number(frameId));
  if (!frame) {
    return null;
  }
  const buf = readFileSync(assertUnderDataDir(frame.path));
  return { body: buf, contentType: "image/jpeg" };
}

export function serveAudio(db, chunkId) {
  const chunk = getAudioChunk(db, Number(chunkId));
  if (!chunk) {
    return null;
  }
  const pcm = readFileSync(assertUnderDataDir(chunk.path));
  return { body: pcmToWav(pcm), contentType: "audio/wav" };
}
