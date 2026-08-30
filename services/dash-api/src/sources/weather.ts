import { config } from "../config.js";
import { fetchJson } from "../cache.js";

export type Weather = {
  valid: boolean;
  temp: number;
  feels: number;
  min: number;
  max: number;
  humidity: number;
  wind: number;
  windDir: number;
  gust: number;
  pressure: number;
  pressureDelta: number;
  rainProb: number;
  code: number;
  desc: string;
  uv: number;
  sunrise: string;
  sunset: string;
  hourly: number[];
  hourNow: number;
  rain15: number[];
  rainIn: number;
  days: { day: string; min: number; max: number; code: number }[];
};

export const emptyWeather: Weather = {
  valid: false,
  temp: 0,
  feels: 0,
  min: 0,
  max: 0,
  humidity: 0,
  wind: 0,
  windDir: 0,
  gust: 0,
  pressure: 0,
  pressureDelta: 0,
  rainProb: 0,
  code: -1,
  desc: "--",
  uv: 0,
  sunrise: "--:--",
  sunset: "--:--",
  hourly: [],
  hourNow: 0,
  rain15: [],
  rainIn: -1,
  days: [],
};

const DESCRIPTIONS: Record<number, string> = {
  0: "ceu limpo",
  1: "poucas nuvens",
  2: "parcialmente nublado",
  3: "nublado",
  45: "nevoeiro",
  48: "nevoeiro gelado",
  51: "garoa fraca",
  53: "garoa",
  55: "garoa forte",
  61: "chuva fraca",
  63: "chuva",
  65: "chuva forte",
  71: "neve fraca",
  73: "neve",
  75: "neve forte",
  80: "pancadas",
  81: "pancadas fortes",
  82: "temporal",
  95: "tempestade",
  96: "tempestade com granizo",
  99: "tempestade severa",
};

const WEEKDAYS = ["dom", "seg", "ter", "qua", "qui", "sex", "sab"];

const hhmm = (iso: string | undefined): string => (iso ? iso.slice(11, 16) : "--:--");

type OpenMeteo = {
  utc_offset_seconds: number;
  current: Record<string, number>;
  hourly: { time: string[]; temperature_2m: number[]; surface_pressure: number[] };
  minutely_15?: { precipitation: number[] };
  daily: {
    time: string[];
    temperature_2m_max: number[];
    temperature_2m_min: number[];
    weather_code: number[];
    precipitation_probability_max: number[];
    uv_index_max: number[];
    sunrise: string[];
    sunset: string[];
  };
};

export const fetchWeather = async (): Promise<Weather & { tzOffset: number }> => {
  const url = new URL("https://api.open-meteo.com/v1/forecast");
  url.searchParams.set("latitude", String(config.lat));
  url.searchParams.set("longitude", String(config.lon));
  url.searchParams.set(
    "current",
    "temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure",
  );
  url.searchParams.set("hourly", "temperature_2m,surface_pressure");
  url.searchParams.set("minutely_15", "precipitation");
  url.searchParams.set(
    "daily",
    "temperature_2m_max,temperature_2m_min,weather_code,precipitation_probability_max,uv_index_max,sunrise,sunset",
  );
  url.searchParams.set("forecast_days", "4");
  url.searchParams.set("forecast_minutely_15", "8");
  url.searchParams.set("timezone", config.timezone);
  url.searchParams.set("wind_speed_unit", "kmh");

  const data = await fetchJson<OpenMeteo>(url.toString());
  const now = new Date(Date.now() + data.utc_offset_seconds * 1000);
  const hourNow = now.getUTCHours();
  const code = data.current.weather_code ?? -1;

  const pressureNow = data.current.surface_pressure ?? 0;
  const pressurePast = data.hourly.surface_pressure[Math.max(hourNow - 3, 0)] ?? pressureNow;

  const rain15 = data.minutely_15?.precipitation?.slice(0, 8) ?? [];
  const rainIndex = rain15.findIndex((mm) => mm > 0.05);

  return {
    valid: true,
    tzOffset: data.utc_offset_seconds,
    temp: data.current.temperature_2m ?? 0,
    feels: data.current.apparent_temperature ?? 0,
    min: data.daily.temperature_2m_min[0] ?? 0,
    max: data.daily.temperature_2m_max[0] ?? 0,
    humidity: data.current.relative_humidity_2m ?? 0,
    wind: data.current.wind_speed_10m ?? 0,
    windDir: data.current.wind_direction_10m ?? 0,
    gust: data.current.wind_gusts_10m ?? 0,
    pressure: pressureNow,
    pressureDelta: pressureNow - pressurePast,
    rainProb: data.daily.precipitation_probability_max[0] ?? 0,
    code,
    desc: DESCRIPTIONS[code] ?? "--",
    uv: data.daily.uv_index_max[0] ?? 0,
    sunrise: hhmm(data.daily.sunrise[0]),
    sunset: hhmm(data.daily.sunset[0]),
    hourly: data.hourly.temperature_2m.slice(0, 24).map((t) => Math.round(t * 10) / 10),
    hourNow,
    rain15: rain15.map((mm) => Math.round(mm * 10) / 10),
    rainIn: rainIndex < 0 ? -1 : rainIndex * 15,
    days: data.daily.time.slice(1, 4).map((day, i) => ({
      day: WEEKDAYS[new Date(`${day}T12:00:00Z`).getUTCDay()] ?? "--",
      min: Math.round(data.daily.temperature_2m_min[i + 1] ?? 0),
      max: Math.round(data.daily.temperature_2m_max[i + 1] ?? 0),
      code: data.daily.weather_code[i + 1] ?? -1,
    })),
  };
};

export type Air = { valid: boolean; aqi: number; pm25: number; pm10: number; label: string };

export const emptyAir: Air = { valid: false, aqi: 0, pm25: 0, pm10: 0, label: "--" };

const aqiLabel = (aqi: number): string => {
  if (aqi <= 20) return "otimo";
  if (aqi <= 40) return "bom";
  if (aqi <= 60) return "moderado";
  if (aqi <= 80) return "ruim";
  if (aqi <= 100) return "muito ruim";
  return "pessimo";
};

export const fetchAir = async (): Promise<Air> => {
  const url = new URL("https://air-quality-api.open-meteo.com/v1/air-quality");
  url.searchParams.set("latitude", String(config.lat));
  url.searchParams.set("longitude", String(config.lon));
  url.searchParams.set("current", "european_aqi,pm2_5,pm10");

  const data = await fetchJson<{ current: Record<string, number> }>(url.toString());
  const aqi = Math.round(data.current.european_aqi ?? 0);

  return {
    valid: true,
    aqi,
    pm25: Math.round((data.current.pm2_5 ?? 0) * 10) / 10,
    pm10: Math.round((data.current.pm10 ?? 0) * 10) / 10,
    label: aqiLabel(aqi),
  };
};
