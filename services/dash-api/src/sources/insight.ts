import { config } from "../config.js";
import { fetchJson } from "../cache.js";
import { clamp } from "../text.js";
import type { Weather } from "./weather.js";

const SYNODIC = 29.530588853;
const NEW_MOON = 947_182_440_000;

export type Moon = { age: number; illum: number; waxing: boolean; name: string };

export const moonPhase = (when = Date.now()): Moon => {
  const age = (((when - NEW_MOON) / 86_400_000) % SYNODIC + SYNODIC) % SYNODIC;

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
    illum: Math.round(((1 - Math.cos((2 * Math.PI * age) / SYNODIC)) / 2) * 100) / 100,
    waxing: age < SYNODIC / 2,
    name,
  };
};

/* gemma models reject a system role and will leak reasoning tokens unless
   thinking is switched off, so everything goes in the user turn. */
export const fetchInsight = async (weather: Weather): Promise<string> => {
  const now = new Date().toLocaleTimeString("pt-BR", {
    timeZone: config.timezone,
    hour: "2-digit",
    minute: "2-digit",
  });

  const context = weather.valid
    ? `Sao ${now} em ${config.city}. Tempo: ${weather.desc}, ${Math.round(weather.temp)} graus, ` +
      `sensacao ${Math.round(weather.feels)}, minima ${Math.round(weather.min)}, maxima ` +
      `${Math.round(weather.max)}, umidade ${weather.humidity} por cento, chance de chuva ` +
      `${weather.rainProb} por cento.`
    : `Sao ${now} em ${config.city}.`;

  const prompt =
    "Voce e o painel de um relogio de mesa. Escreva UMA frase em portugues do Brasil, no " +
    "maximo 18 palavras, util ou espirituosa, sobre a hora e o clima. Sem emojis, sem aspas, " +
    `sem explicacao.\n\n${context}`;

  const body = await fetchJson<{ message?: { content?: string } }>(config.ollamaUrl, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      model: config.ollamaModel,
      stream: false,
      think: false,
      options: { num_predict: 80, temperature: 0.8 },
      messages: [{ role: "user", content: prompt }],
    }),
  });

  const text = body.message?.content ?? "";
  if (!text.trim()) throw new Error("resposta vazia");
  return clamp(text.replace(/^["']|["']$/g, ""), 180);
};
