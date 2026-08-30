import { config } from "../config.js";
import { fetchJson, fetchText } from "../cache.js";
import { clamp } from "../text.js";

export const fetchNews = async (): Promise<string[]> => {
  const xml = await fetchText(config.newsUrl);
  const items = [...xml.matchAll(/<item>[\s\S]*?<title>([\s\S]*?)<\/title>/g)];

  const headlines = items
    .map((match) => (match[1] ?? "").replace(/<!\[CDATA\[|\]\]>/g, ""))
    .map((title) =>
      title
        .replace(/&amp;/g, "&")
        .replace(/&quot;/g, '"')
        .replace(/&#39;|&apos;/g, "'")
        .replace(/&lt;/g, "<")
        .replace(/&gt;/g, ">"),
    )
    .map((title) => clamp(title, 110))
    .filter((title) => title.length > 0)
    .slice(0, 3);

  if (headlines.length === 0) throw new Error("feed sem itens");
  return headlines;
};

export type Market = {
  valid: boolean;
  usd: number;
  usdPct: number;
  eur: number;
  eurPct: number;
  btc: number;
  btcPct: number;
};

export const emptyMarket: Market = {
  valid: false,
  usd: 0,
  usdPct: 0,
  eur: 0,
  eurPct: 0,
  btc: 0,
  btcPct: 0,
};

export const fetchMarket = async (): Promise<Market> => {
  type Quote = { bid: string; pctChange: string };
  const data = await fetchJson<Record<string, Quote>>(
    "https://economia.awesomeapi.com.br/last/USD-BRL,EUR-BRL,BTC-BRL",
  );

  const bid = (key: string) => Number(data[key]?.bid ?? 0);
  const pct = (key: string) => Number(data[key]?.pctChange ?? 0);

  return {
    valid: true,
    usd: bid("USDBRL"),
    usdPct: pct("USDBRL"),
    eur: bid("EURBRL"),
    eurPct: pct("EURBRL"),
    btc: bid("BTCBRL"),
    btcPct: pct("BTCBRL"),
  };
};

export type Rates = { valid: boolean; selic: number; cdi: number; ipca: number };

export const emptyRates: Rates = { valid: false, selic: 0, cdi: 0, ipca: 0 };

export const fetchRates = async (): Promise<Rates> => {
  const data = await fetchJson<{ nome: string; valor: number }[]>(
    "https://brasilapi.com.br/api/taxas/v1",
  );
  const find = (name: string) => data.find((rate) => rate.nome === name)?.valor ?? 0;

  return { valid: true, selic: find("Selic"), cdi: find("CDI"), ipca: find("IPCA") };
};

export type Holiday = { valid: boolean; name: string; date: string; daysLeft: number };

export const emptyHoliday: Holiday = { valid: false, name: "--", date: "--", daysLeft: 0 };

export const fetchHoliday = async (): Promise<Holiday> => {
  const today = new Date();
  const iso = today.toISOString().slice(0, 10);

  const load = (year: number) =>
    fetchJson<{ date: string; name: string }[]>(
      `https://brasilapi.com.br/api/feriados/v1/${year}`,
    );

  const list = [...(await load(today.getUTCFullYear())), ...(await load(today.getUTCFullYear() + 1))];
  const next = list.find((holiday) => holiday.date >= iso);
  if (!next) throw new Error("sem feriado futuro");

  const days = Math.round(
    (Date.parse(`${next.date}T12:00:00Z`) - Date.parse(`${iso}T12:00:00Z`)) / 86_400_000,
  );

  return { valid: true, name: clamp(next.name, 48), date: next.date, daysLeft: days };
};

export type History = { valid: boolean; year: number; text: string };

export const emptyHistory: History = { valid: false, year: 0, text: "--" };

export const fetchHistory = async (): Promise<History> => {
  const now = new Date();
  const url = `https://pt.wikipedia.org/api/rest_v1/feed/onthisday/selected/${now.getMonth() + 1}/${now.getDate()}`;
  const data = await fetchJson<{ selected: { year: number; text: string }[] }>(url);

  const events = data.selected ?? [];
  const pick = events[Math.floor(Math.random() * events.length)];
  if (!pick) throw new Error("sem eventos");

  return { valid: true, year: pick.year, text: clamp(pick.text, 180) };
};

export type Space = { valid: boolean; people: number; lat: number; lon: number };

export const emptySpace: Space = { valid: false, people: 0, lat: 0, lon: 0 };

export const fetchSpace = async (): Promise<Space> => {
  const crew = await fetchJson<{ number: number }>("http://api.open-notify.org/astros.json");
  const iss = await fetchJson<{ iss_position: { latitude: string; longitude: string } }>(
    "http://api.open-notify.org/iss-now.json",
  );

  return {
    valid: true,
    people: crew.number,
    lat: Math.round(Number(iss.iss_position.latitude) * 10) / 10,
    lon: Math.round(Number(iss.iss_position.longitude) * 10) / 10,
  };
};
