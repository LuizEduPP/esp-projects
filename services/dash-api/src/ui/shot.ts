import { mkdtemp, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "playwright";
import { buildPages } from "./pages.js";
import { previewCss, renderJs } from "./renderjs.js";
import { themes } from "./themes.js";

const screen = process.argv[2] ?? "weather";
const out = process.argv[3] ?? "/tmp/themes-shot.png";
const only = process.argv[4];

const src = process.env.DASH_API_URL ?? "https://dash-esp32.kmali.online";
const DATA = await fetch(src + "/dash")
  .then((r) => r.json())
  .catch(() => ({}));

const cards = themes
  .filter((t) => !only || only.split(",").includes(t.id))
  .map((t) => {
    const page = buildPages(t).find((p) => p.id === screen) ?? buildPages(t)[0]!;
    return `<figure><div class="slot" data-page='${JSON.stringify(page).replace(/'/g, "&#39;")}'></div><figcaption>${t.name}</figcaption></figure>`;
  })
  .join("");

const html = `<!doctype html><meta charset="utf-8"><style>
body{margin:0;padding:16px;background:#0b0b0d;color:#e5e7eb;font:12px system-ui;}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:16px;}
figure{margin:0}figcaption{margin-top:6px;color:#9ca3af;font-size:11px}
${previewCss}
</style><div class="grid">${cards}</div><script>
${renderJs}
const DATA = ${JSON.stringify(DATA)};
for (const el of document.querySelectorAll('.slot')) {
  el.replaceWith(renderPage(JSON.parse(el.dataset.page), DATA));
}
</script>`;

const dir = await mkdtemp(join(tmpdir(), "shot-"));
const file = join(dir, "s.html");
await writeFile(file, html);

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 660, height: 800 } });
await page.goto("file://" + file);
await page.waitForTimeout(300);
await page.screenshot({ path: out, fullPage: true });
await browser.close();
console.log(out);
