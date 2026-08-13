// Steam エンドポイント向けの小さな HTTP ヘルパ。 UA 明示 + polite delay のみを責務とする。

import { setTimeout as sleep } from 'node:timers/promises';

import type { SteamHttpOptions } from './types.js';

export const STEAM_USER_AGENT = 'LUDIARS-Canalis-Crawler/0.1 (+https://github.com/LUDIARS/Canalis)';

export type SteamHttp = {
  getJson: (url: string) => Promise<unknown>;
  getText: (url: string) => Promise<string>;
  politeWait: () => Promise<void>;
};

/** @implements SPEC-CRAWL-STEAM — SteamHttpOptions から実 HTTP 層を組み立てる。 */
export function buildSteamHttp(opts: SteamHttpOptions = {}): SteamHttp {
  const doFetch = opts.fetchImpl ?? fetch;
  const delayMs = opts.politeDelayMs ?? 1200;
  if (!Number.isSafeInteger(delayMs) || delayMs < 0) {
    throw new Error('steam politeDelayMs must be a non-negative integer');
  }
  const wait = opts.sleepImpl ?? ((ms: number) => sleep(ms));

  /** @implements SPEC-CRAWL-STEAM */
  const get = async (url: string): Promise<Response> => {
    const res = await doFetch(url, {
      headers: { 'User-Agent': STEAM_USER_AGENT },
      redirect: 'error',
    });
    if (!res.ok) throw new Error(`steam fetch HTTP ${res.status}`);
    return res;
  };

  return {
    getJson: async (url) => (await get(url)).json(),
    getText: async (url) => (await get(url)).text(),
    politeWait: () => wait(delayMs),
  };
}
