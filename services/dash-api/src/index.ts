import Fastify from "fastify";

import { config } from "./config.js";
import { Source } from "./cache.js";
import {
  emptyHistory,
  emptyHoliday,
  emptyMarket,
  emptyRates,
  emptySpace,
  fetchHistory,
  fetchHoliday,
  fetchMarket,
  fetchNews,
  fetchRates,
  fetchSpace,
} from "./sources/brasil.js";
import { emptyDev, fetchDev } from "./sources/github.js";
import { fetchInsight, moonPhase } from "./sources/insight.js";
import { emptyAir, emptyWeather, fetchAir, fetchWeather } from "./sources/weather.js";
import { hiddenPages, previewDoc, setUi, themeList, uiDoc, uiVersion } from "./ui/state.js";
import { editorPage } from "./ui/editor.js";

const MINUTE = 60_000;

const weather = new Source("clima", fetchWeather, 10 * MINUTE, {
  ...emptyWeather,
  tzOffset: 0,
});
const air = new Source("ar", fetchAir, 30 * MINUTE, emptyAir);
const news = new Source("noticias", fetchNews, 15 * MINUTE, [] as string[]);
const market = new Source("cotacoes", fetchMarket, 10 * MINUTE, emptyMarket);
const rates = new Source("taxas", fetchRates, 6 * 60 * MINUTE, emptyRates);
const holiday = new Source("feriado", fetchHoliday, 12 * 60 * MINUTE, emptyHoliday);
const history = new Source("historia", fetchHistory, 6 * 60 * MINUTE, emptyHistory);
const space = new Source("espaco", fetchSpace, 5 * MINUTE, emptySpace);
const dev = new Source("github", fetchDev, 10 * MINUTE, emptyDev);
const insight = new Source("ai", () => fetchInsight(weather.get()), 10 * MINUTE, "");

const sources = [weather, air, news, market, rates, holiday, history, space, dev, insight];

const app = Fastify({ logger: { level: "warn" } });

app.get("/health", async () => ({
  ok: true,
  ages: Object.fromEntries(sources.map((source) => [source.name, source.age])),
}));

app.get("/dash", async () => {
  const current = weather.get();

  return {
    now: Math.floor(Date.now() / 1000),
    tz: current.tzOffset,
    city: config.city,
    weather: {
      valid: current.valid,
      temp: current.temp,
      feels: current.feels,
      min: current.min,
      max: current.max,
      hum: current.humidity,
      wind: current.wind,
      dir: current.windDir,
      gust: current.gust,
      pres: current.pressure,
      dpres: Math.round(current.pressureDelta * 10) / 10,
      rainp: current.rainProb,
      code: current.code,
      desc: current.desc,
      uv: current.uv,
      sunrise: current.sunrise,
      sunset: current.sunset,
      hourly: current.hourly,
      hournow: current.hourNow,
      rain15: current.rain15,
      rainin: current.rainIn,
      days: current.days,
    },
    air: air.get(),
    moon: moonPhase(),
    news: news.get(),
    market: market.get(),
    rates: rates.get(),
    holiday: holiday.get(),
    history: history.get(),
    space: space.get(),
    dev: dev.get(),
    ai: insight.get(),
  };
});

app.post("/refresh", async () => {
  await Promise.all(sources.map((source) => source.refresh(true)));
  return { ok: true };
});

app.get("/ui", async () => uiDoc());

app.get("/ui/version", async () => uiVersion());

app.get("/ui/themes", async () => ({ themes: themeList(), hidden: hiddenPages() }));

app.get<{ Params: { id: string } }>("/ui/preview/:id", async (req) => previewDoc(req.params.id));

app.post<{ Body: { theme?: string; hidden?: string[] } }>("/ui", async (req) => {
  const doc = setUi(req.body ?? {});
  return { ok: true, version: doc.version, theme: doc.theme };
});

app.get("/", async (_req, reply) => {
  reply.type("text/html; charset=utf-8");
  return editorPage();
});

for (const source of sources) source.start();

await app.listen({ port: config.port, host: config.host });
console.log(`dash-api em http://${config.host}:${config.port}/dash`);
