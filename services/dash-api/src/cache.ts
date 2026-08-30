type Loader<T> = () => Promise<T>;

/* Keeps the last good value forever and refreshes it on a timer, so a GET from
   the board never waits on an upstream API and a flaky source degrades into
   stale data instead of an empty screen. */
export class Source<T> {
  private value: T;
  private updatedAt = 0;
  private inFlight: Promise<void> | null = null;

  constructor(
    readonly name: string,
    private readonly load: Loader<T>,
    private readonly ttlMs: number,
    fallback: T,
  ) {
    this.value = fallback;
  }

  get(): T {
    return this.value;
  }

  get age(): number {
    return this.updatedAt === 0 ? -1 : Math.round((Date.now() - this.updatedAt) / 1000);
  }

  async refresh(force = false): Promise<void> {
    if (!force && this.updatedAt > 0 && Date.now() - this.updatedAt < this.ttlMs) return;
    if (this.inFlight) return this.inFlight;

    this.inFlight = (async () => {
      try {
        this.value = await this.load();
        this.updatedAt = Date.now();
      } catch (error) {
        const reason = error instanceof Error ? error.message : String(error);
        console.warn(`[${this.name}] ${reason}`);
      } finally {
        this.inFlight = null;
      }
    })();

    return this.inFlight;
  }

  start(): void {
    void this.refresh(true);
    setInterval(() => void this.refresh(true), this.ttlMs).unref();
  }
}

export const fetchJson = async <T>(url: string, init?: RequestInit): Promise<T> => {
  const response = await fetch(url, {
    ...init,
    signal: AbortSignal.timeout(20_000),
    headers: { "user-agent": "dash-api", accept: "application/json", ...init?.headers },
  });
  if (!response.ok) throw new Error(`HTTP ${response.status} on ${new URL(url).host}`);
  return (await response.json()) as T;
};

export const fetchText = async (url: string): Promise<string> => {
  const response = await fetch(url, {
    signal: AbortSignal.timeout(20_000),
    headers: { "user-agent": "dash-api" },
  });
  if (!response.ok) throw new Error(`HTTP ${response.status} on ${new URL(url).host}`);
  return await response.text();
};
