import { config } from "../config.js";
import { fetchJson } from "../cache.js";
import { clamp } from "../text.js";

export type Dev = {
  valid: boolean;
  today: number;
  week: number;
  activeDays: number;
  repos: number;
  followers: number;
  repo: string;
  private: boolean;
};

export const emptyDev: Dev = {
  valid: false,
  today: 0,
  week: 0,
  activeDays: 0,
  repos: 0,
  followers: 0,
  repo: "--",
  private: false,
};

const startOfWeekAgo = (): string => {
  const date = new Date();
  date.setUTCDate(date.getUTCDate() - 6);
  date.setUTCHours(0, 0, 0, 0);
  return date.toISOString();
};

const fetchWithToken = async (): Promise<Dev> => {
  const query = `
    query($login: String!, $from: DateTime!) {
      user(login: $login) {
        followers { totalCount }
        repositories(ownerAffiliations: OWNER, first: 1) { totalCount }
        contributionsCollection(from: $from) {
          totalCommitContributions
          restrictedContributionsCount
          contributionCalendar {
            weeks { contributionDays { date contributionCount } }
          }
        }
      }
    }`;

  type Response = {
    data: {
      user: {
        followers: { totalCount: number };
        repositories: { totalCount: number };
        contributionsCollection: {
          totalCommitContributions: number;
          restrictedContributionsCount: number;
          contributionCalendar: {
            weeks: { contributionDays: { date: string; contributionCount: number }[] }[];
          };
        };
      };
    };
    errors?: { message: string }[];
  };

  const body = await fetchJson<Response>("https://api.github.com/graphql", {
    method: "POST",
    headers: {
      authorization: `bearer ${config.githubToken}`,
      "content-type": "application/json",
    },
    body: JSON.stringify({
      query,
      variables: { login: config.githubUser, from: startOfWeekAgo() },
    }),
  });

  if (body.errors?.length) throw new Error(body.errors[0]?.message ?? "graphql");

  const user = body.data.user;
  const days = user.contributionsCollection.contributionCalendar.weeks.flatMap(
    (week) => week.contributionDays,
  );
  const today = new Date().toISOString().slice(0, 10);

  return {
    valid: true,
    today: days.find((day) => day.date === today)?.contributionCount ?? 0,
    week:
      user.contributionsCollection.totalCommitContributions +
      user.contributionsCollection.restrictedContributionsCount,
    activeDays: days.filter((day) => day.contributionCount > 0).length,
    repos: user.repositories.totalCount,
    followers: user.followers.totalCount,
    repo: await lastRepo(),
    private: true,
  };
};

const lastRepo = async (): Promise<string> => {
  try {
    const events = await fetchJson<{ repo?: { name: string } }[]>(
      `https://api.github.com/users/${config.githubUser}/events/public?per_page=1`,
      config.githubToken ? { headers: { authorization: `bearer ${config.githubToken}` } } : {},
    );
    const name = events[0]?.repo?.name ?? "";
    return clamp(name.split("/").pop() ?? "--", 28);
  } catch {
    return "--";
  }
};

const fetchPublic = async (): Promise<Dev> => {
  type Event = {
    type: string;
    created_at: string;
    repo?: { name: string };
    payload?: { size?: number };
  };

  const [profile, events] = await Promise.all([
    fetchJson<{ public_repos: number; followers: number }>(
      `https://api.github.com/users/${config.githubUser}`,
    ),
    fetchJson<Event[]>(
      `https://api.github.com/users/${config.githubUser}/events/public?per_page=100`,
    ),
  ]);

  const today = new Date().toISOString().slice(0, 10);
  const weekAgo = startOfWeekAgo().slice(0, 10);
  const days = new Set<string>();
  let commitsToday = 0;
  let commitsWeek = 0;

  for (const event of events) {
    if (event.type !== "PushEvent") continue;
    const day = event.created_at.slice(0, 10);
    const size = event.payload?.size ?? 1;

    if (day === today) commitsToday += size;
    if (day >= weekAgo) {
      commitsWeek += size;
      days.add(day);
    }
  }

  return {
    valid: true,
    today: commitsToday,
    week: commitsWeek,
    activeDays: days.size,
    repos: profile.public_repos,
    followers: profile.followers,
    repo: clamp(events[0]?.repo?.name.split("/").pop() ?? "--", 28),
    private: false,
  };
};

export const fetchDev = async (): Promise<Dev> =>
  config.githubToken ? await fetchWithToken() : await fetchPublic();
