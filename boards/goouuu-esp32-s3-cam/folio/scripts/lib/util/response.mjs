export function sendJson(res, status, body) {
  res.writeHead(status, { "Content-Type": "application/json" });
  res.end(JSON.stringify(body));
}

export function sendBytes(res, status, body, contentType, cache = "private, max-age=3600") {
  res.writeHead(status, { "Content-Type": contentType, "Cache-Control": cache });
  res.end(body);
}

export function errMsg(err) {
  return [err?.message, err?.cause?.message, err?.cause?.code].filter(Boolean).join(" — ");
}
