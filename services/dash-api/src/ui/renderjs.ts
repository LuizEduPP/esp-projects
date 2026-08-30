// Renderizador JS do documento de UI. E o mesmo codigo usado pelo editor e pelo
// export estatico, para que preview e placa nao possam divergir por descuido.
export const renderJs = String.raw`
const px = (n) => n + "px";
const get = (obj, path) => path.split(".").reduce((acc, k) => (acc == null ? acc : acc[k]), obj);

function fmt(spec, value) {
  if (value === undefined || value === null) return "--";
  if (!spec) return String(value);
  return spec.replace(/%[-0-9.]*[sdf]/, (m) => {
    if (m.endsWith("s")) return String(value);
    if (m.endsWith("d")) return String(Math.round(Number(value)));
    const dec = /\.(\d)/.exec(m);
    return Number(value).toFixed(dec ? Number(dec[1]) : 0);
  }).replace(/%%/g, "%");
}

function bgStyle(bg) {
  if (bg.kind === "gradient") return "background:linear-gradient(160deg," + bg.from + "," + bg.to + ")";
  if (bg.kind === "grid")
    return "background:" + bg.from + ";background-image:linear-gradient(" + bg.line +
      " 1px,transparent 1px),linear-gradient(90deg," + bg.line +
      " 1px,transparent 1px);background-size:" + bg.step + "px " + bg.step + "px";
  if (bg.kind === "dots")
    return "background:" + bg.from + ";background-image:radial-gradient(" + bg.line +
      " 1px,transparent 1px);background-size:" + bg.step + "px " + bg.step + "px";
  return "background:" + bg.from;
}

function renderPage(page, DATA) {
  const el = document.createElement("div");
  el.className = "screen";
  el.setAttribute("style", bgStyle(page.bg));

  for (const b of page.bg.blobs || []) {
    const d = document.createElement("div");
    d.setAttribute("style", "left:" + px(b.x - b.r) + ";top:" + px(b.y - b.r) +
      ";width:" + px(b.r * 2) + ";height:" + px(b.r * 2) + ";border-radius:50%;background:" +
      b.color + ";opacity:" + (b.opa / 255) + ";filter:blur(14px)");
    el.appendChild(d);
  }

  for (const it of page.items) el.appendChild(renderItem(it, DATA));
  return el;
}

function renderItem(it, DATA) {
  const d = document.createElement("div");
  const at = (s) => d.setAttribute("style", s);

  if (it.t === "label") {
    const raw = it.bind ? get(DATA, it.bind) : it.text;
    d.textContent = it.text !== undefined && !it.bind ? it.text : fmt(it.fmt, raw);
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.w) +
      ";font-size:" + px(it.font) + ";color:" + it.color +
      ";text-align:" + (it.align || "center") + ";line-height:1.12;overflow:hidden;" +
      (it.h !== undefined ? "height:" + px(it.h) + ";" : "") +
      (it.wrap ? "" : "white-space:nowrap;text-overflow:ellipsis;"));
    return d;
  }

  if (it.t === "plate") {
    const style = it.style === "glass"
      ? "background:" + it.color + "22;border:1px solid " + it.color + "3a"
      : it.style === "solid" ? "background:" + it.color
      : it.style === "outline" ? "border:1px solid " + it.color : "";
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.w) +
      ";height:" + px(it.h) + ";border-radius:10px;" + style);
    return d;
  }

  if (it.t === "rule") {
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.w) +
      ";height:" + px(it.h) + ";background:" + it.color);
    return d;
  }

  if (it.t === "arc") {
    const v = it.bind ? Number(get(DATA, it.bind)) || 0 : (it.value || 0);
    const min = it.min == null ? 0 : it.min, max = it.max == null ? 100 : it.max;
    const pct = Math.min(Math.max((v - min) / (max - min || 1), 0), 1);
    const w = it.width || 8;
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.size) +
      ";height:" + px(it.size) + ";border-radius:50%;background:conic-gradient(" +
      it.color + " 0 " + (pct * 75).toFixed(1) + "%," + it.track + " 0 75%,transparent 0);" +
      "-webkit-mask:radial-gradient(circle,transparent " + px(it.size / 2 - w) +
      ",#000 " + px(it.size / 2 - w) + ");mask:radial-gradient(circle,transparent " +
      px(it.size / 2 - w) + ",#000 " + px(it.size / 2 - w) + ");transform:rotate(135deg)");
    return d;
  }

  if (it.t === "needle") {
    const deg = (Number(get(DATA, it.bind)) || 0) % 360;
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.size) +
      ";height:" + px(it.size) + ";border-radius:50%;border:1px solid " + it.track);
    const n = document.createElement("div");
    n.setAttribute("style", "left:" + px(it.size / 2 - 2) + ";top:6px;width:4px;height:" +
      px(it.size / 2 - 6) + ";background:" + it.color + ";border-radius:2px;transform-origin:50% 100%;" +
      "transform:rotate(" + deg + "deg)");
    d.appendChild(n);
    return d;
  }

  if (it.t === "bar") {
    const v = Number(it.bind ? get(DATA, it.bind) : it.value) || 0;
    const min = it.min == null ? 0 : it.min, max = it.max == null ? 100 : it.max;
    const pct = Math.min(Math.max((v - min) / (max - min || 1), 0), 1);
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.w) + ";height:" + px(it.h) +
      ";border-radius:" + px(it.h / 2) + ";background:" + it.track);
    const f = document.createElement("div");
    f.setAttribute("style", "left:0;top:0;width:" + px(it.w * pct) + ";height:" + px(it.h) +
      ";border-radius:" + px(it.h / 2) + ";background:" + it.color);
    d.appendChild(f);
    return d;
  }

  if (it.t === "chart") {
    const arr = get(DATA, it.bind);
    const vals = Array.isArray(arr) && arr.length ? arr : [3, 5, 4, 7, 9, 8, 6, 4, 3, 5, 7, 6];
    const lo = Math.min.apply(null, vals), hi = Math.max.apply(null, vals);
    const span = (hi - lo) || 1;
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.w) + ";height:" + px(it.h));
    const step = it.w / vals.length;
    vals.forEach((v, i) => {
      const h = Math.max(2, ((v - lo) / span) * it.h);
      const b = document.createElement("div");
      b.setAttribute("style", "left:" + px(i * step) + ";top:" + px(it.h - h) + ";width:" +
        px(Math.max(1, step - 1)) + ";height:" + px(h) + ";background:" + it.color +
        ";opacity:.9;border-radius:1px");
      d.appendChild(b);
    });
    return d;
  }

  if (it.t === "icon") {
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.size) + ";height:" + px(it.size));
    const sun = document.createElement("div");
    sun.setAttribute("style", "left:" + px(it.size / 6) + ";top:" + px(it.size / 6) + ";width:" +
      px(it.size / 2) + ";height:" + px(it.size / 2) + ";border-radius:50%;background:" + it.color);
    const cloud = document.createElement("div");
    cloud.setAttribute("style", "left:" + px(it.size / 5) + ";top:" + px(it.size / 2) + ";width:" +
      px(it.size * 3 / 5) + ";height:" + px(it.size / 4) + ";border-radius:99px;background:" +
      (it.color2 || "#aaa"));
    d.appendChild(sun); d.appendChild(cloud);
    return d;
  }

  if (it.t === "moon") {
    at("left:" + px(it.x) + ";top:" + px(it.y) + ";width:" + px(it.size) + ";height:" +
      px(it.size) + ";border-radius:50%;background:" + it.color + ";opacity:.28");
    const lit = document.createElement("div");
    const pct = (Number(get(DATA, "moon.illum")) || 60) / 100;
    lit.setAttribute("style", "left:0;top:0;width:" + px(it.size * pct) + ";height:" + px(it.size) +
      ";border-radius:50%;background:" + it.color);
    d.appendChild(lit);
    return d;
  }

  return d;
}
`;

export const previewCss = String.raw`
  .screen { position:relative; width:128px; height:128px; overflow:hidden;
    font-family:"Montserrat","Segoe UI",system-ui,sans-serif; -webkit-font-smoothing:antialiased; }
  .screen * { position:absolute; margin:0; padding:0; box-sizing:border-box; }
`;

export const sampleData = {
  city: "CESARIO LANGE",
  time: "14:32",
  weekday: "DOM",
  date: "30 de agosto",
  ai: "Tarde estavel, sem chuva ate a noite. Bom momento para focar.",
  weather: {
    temp: 24, feels: 25, min: 17, max: 29, hum: 68, wind: 12, dir: 135, gust: 22,
    pres: 1014, rainp: 20, rainin: 45, code: 2, desc: "parcialmente nublado", uv: 7,
    sunrise: "06:21", sunset: "18:04",
    hourly: [19, 18, 18, 20, 23, 26, 28, 29, 28, 26, 24, 22, 21, 20, 19, 19],
    rain15: [0, 0, 1, 3, 6, 4, 2, 0],
    days: [
      { day: "SEG", min: 16, max: 27, code: 3 },
      { day: "TER", min: 18, max: 30, code: 1 },
      { day: "QUA", min: 17, max: 25, code: 61 },
    ],
  },
  air: { aqi: 34, pm25: 8, pm10: 14, label: "boa" },
  moon: { illum: 62, name: "crescente gibosa" },
  sun: { progress: 62, daylight: "11h 43min" },
  news: [
    "Governo anuncia novo pacote de investimento em energia solar no Nordeste",
    "Selic mantida em reuniao do Copom desta quarta-feira",
    "Pesquisa aponta queda no desmatamento pelo terceiro mes seguido",
  ],
  market: { usd: 5.42, eur: 5.88, btc: 412350 },
  rates: { selic: 10.5, cdi: 10.4, ipca: 4.32 },
  holiday: { name: "Independencia do Brasil", date: "07/09/2026", daysLeft: 8 },
  history: { year: 1930, text: "Inaugurada a primeira transmissao de radio em onda curta do pais." },
  space: { people: 7, lat: -12, lon: 148 },
  dev: { today: 6, week: 23, activeDays: 5, repos: 21, repo: "015-esp-projects" },
  timer: { clock: "18:42", mode: "foco", percent: 25 },
  net: { ssid: "hotspot", ip: "10.31.254.103", rssi: -58 },
  sys: { heap: 187, uptime: "2h 14m" },
};
