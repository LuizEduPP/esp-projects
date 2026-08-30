import { config } from "../config.js";
import { fetchJson } from "../cache.js";
import { clamp } from "../text.js";
import type { Air, Weather } from "./weather.js";
import type { Dev } from "./github.js";
import type { History, Holiday, Market, Rates, Space } from "./brasil.js";

const SYNODIC = 29.530588853;
const NEW_MOON = 947_182_440_000;

export type Moon = {
  age: number;
  illum: number;
  waxing: boolean;
  name: string;
};

export const moonPhase = (when = Date.now()): Moon => {
  const age =
    ((((when - NEW_MOON) / 86_400_000) % SYNODIC) + SYNODIC) % SYNODIC;

  const name =
    age < 1 || age > SYNODIC - 1
      ? "nova"
      : age < 6.4
        ? "crescente"
        : age < 8.4
          ? "quarto crescente"
          : age < 13.8
            ? "gibosa crescente"
            : age < 15.8
              ? "cheia"
              : age < 21.1
                ? "gibosa minguante"
                : age < 23.1
                  ? "quarto minguante"
                  : "minguante";

  return {
    age: Math.round(age * 10) / 10,
    illum:
      Math.round(((1 - Math.cos((2 * Math.PI * age) / SYNODIC)) / 2) * 100) /
      100,
    waxing: age < SYNODIC / 2,
    name,
  };
};

export type InsightContext = {
  weather: Weather;
  air: Air;
  news: string[];
  market: Market;
  rates: Rates;
  holiday: Holiday;
  history: History;
  space: Space;
  dev: Dev;
};

export const fetchInsight = async (ctx: InsightContext): Promise<string> => {
  const stamp = new Date().toLocaleString("pt-BR", {
    timeZone: config.timezone,
    weekday: "long",
    day: "2-digit",
    month: "long",
    hour: "2-digit",
    minute: "2-digit",
  });

  const { weather: w, air, market, rates, holiday, history, space, dev } = ctx;
  const moon = moonPhase();
  const round = (value: number) => Math.round(value);

  const lines = [`Agora: ${stamp} em ${config.city}.`];

  if (w.valid) {
    lines.push(
      `Clima: ${w.desc}, ${round(w.temp)} graus, sensacao ${round(w.feels)}, minima ` +
        `${round(w.min)}, maxima ${round(w.max)}, umidade ${w.humidity} por cento, vento ` +
        `${round(w.wind)} km/h, chance de chuva ${w.rainProb} por cento, indice UV ${w.uv}, ` +
        `nascer do sol ${w.sunrise}, por do sol ${w.sunset}.`,
    );
  }
  if (air.valid)
    lines.push(
      `Ar: qualidade ${air.label}, indice ${air.aqi}, PM2.5 ${air.pm25}.`,
    );
  lines.push(
    `Lua: ${moon.name}, ${Math.round(moon.illum * 100)} por cento iluminada.`,
  );
  if (market.valid) {
    lines.push(
      `Mercado: dolar ${market.usd.toFixed(2)} reais (${market.usdPct.toFixed(1)} por cento), ` +
        `euro ${market.eur.toFixed(2)}, bitcoin ${round(market.btc)} dolares ` +
        `(${market.btcPct.toFixed(1)} por cento).`,
    );
  }
  if (rates.valid) {
    lines.push(
      `Taxas: Selic ${rates.selic}, CDI ${rates.cdi}, IPCA ${rates.ipca} por cento.`,
    );
  }
  if (holiday.valid) {
    lines.push(
      `Proximo feriado: ${holiday.name} em ${holiday.date}, ${holiday.daysLeft} dias.`,
    );
  }
  if (history.valid)
    lines.push(`Efemeride de hoje: em ${history.year}, ${history.text}`);
  if (space.valid)
    lines.push(`Espaco: ${space.people} pessoas em orbita agora.`);
  if (dev.valid) {
    lines.push(
      `GitHub do dono: ${dev.today} commits hoje, ${dev.week} na semana, ultimo repo ${dev.repo}.`,
    );
  }
  if (ctx.news.length) {
    lines.push(`Manchetes do Brasil: ${ctx.news.slice(0, 3).join(" | ")}`);
  }

  const prompt =
    "Voce e o painel de um relogio de mesa do Luiz. Abaixo esta tudo que o painel sabe agora. " +
    "Escreva UMA frase em portugues do Brasil, no maximo 18 palavras, util ou espirituosa, " +
    "conectando o momento a UM ou DOIS desses dados. Nao liste os dados, nao repita numeros " +
    "demais. Sem emojis, sem aspas, sem explicacao.\n\n" +
    lines.join("\n");

  const body = await fetchJson<{ message?: { content?: string } }>(
    config.aiUrl,
    {
      method: "POST",
      headers: {
        "content-type": "application/json",
        ...(config.aiToken
          ? { authorization: `Bearer ${config.aiToken}` }
          : {}),
      },
      body: JSON.stringify({
        model: config.aiModel,
        stream: false,
        think: false,
        options: { num_predict: 80, temperature: 0.8 },
        messages: [{ role: "user", content: prompt }],
      }),
    },
    60_000,
  );

  const text = body.message?.content ?? "";
  if (!text.trim()) throw new Error("resposta vazia");
  return clamp(text.replace(/^["']|["']$/g, ""), 180);
};
