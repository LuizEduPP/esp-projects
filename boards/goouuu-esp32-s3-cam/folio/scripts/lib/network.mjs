import { networkInterfaces } from "node:os";
import { CFG } from "./config.mjs";

function normalizeIp(raw) {
  if (!raw) {
    return null;
  }
  let ip = String(raw);
  if (ip.startsWith("::ffff:")) {
    ip = ip.slice(7);
  }
  if (ip.includes(":")) {
    return null;
  }
  return ip;
}

function ipv4Prefix(ip, bits = 24) {
  const parts = ip.split(".").map(Number);
  if (parts.length !== 4 || parts.some((n) => Number.isNaN(n))) {
    return null;
  }
  if (bits === 24) {
    return `${parts[0]}.${parts[1]}.${parts[2]}`;
  }
  return ip;
}

function sameSubnet(a, b, bits = 24) {
  const pa = ipv4Prefix(a, bits);
  const pb = ipv4Prefix(b, bits);
  return pa && pb && pa === pb;
}

/** All non-internal IPv4 LAN addresses for this host. */
export function listLanUrls(port = CFG.port) {
  const urls = [];
  for (const ifaces of Object.values(networkInterfaces())) {
    for (const iface of ifaces ?? []) {
      if (iface.family === "IPv4" && !iface.internal) {
        urls.push(`http://${iface.address}:${port}`);
      }
    }
  }
  return urls;
}

/**
 * Pick brain URL reachable from the ESP's subnet.
 * Prefers node.brainUrl from config, else same /24 as client, else first LAN URL.
 */
export function brainUrlForClient(clientIpRaw) {
  const configured = CFG.nodeBrainUrl?.trim();
  if (configured) {
    return configured.replace(/\/+$/, "");
  }

  const clientIp = normalizeIp(clientIpRaw);
  if (clientIp) {
    for (const ifaces of Object.values(networkInterfaces())) {
      for (const iface of ifaces ?? []) {
        if (iface.family === "IPv4" && !iface.internal && sameSubnet(clientIp, iface.address)) {
          return `http://${iface.address}:${CFG.port}`;
        }
      }
    }
  }

  const lan = listLanUrls();
  return lan[0] ?? `http://127.0.0.1:${CFG.port}`;
}
