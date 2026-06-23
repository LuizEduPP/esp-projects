/** Normalize any legacy URL to OpenAI-compatible base: https://host/v1 */

const ENDPOINT_SUFFIX =
  /\/v1\/(?:chat\/completions|embeddings|models|rerank|audio\/transcriptions|images\/generations)\/?$/i;

export function normalizeOpenAiBase(url) {
  let base = String(url ?? "").trim().replace(/\/+$/, "");
  if (!base) {
    return "http://127.0.0.1:1234/v1";
  }
  base = base.replace(ENDPOINT_SUFFIX, "");
  if (!/\/v1$/i.test(base)) {
    base = `${base}/v1`;
  }
  return base;
}

export function joinOpenAiPath(baseUrl, endpoint) {
  const base = normalizeOpenAiBase(baseUrl);
  const path = String(endpoint).replace(/^\/+/, "");
  return `${base}/${path}`;
}
