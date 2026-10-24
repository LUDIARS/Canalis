// Steam エンドポイント向けの小さな HTTP ヘルパ。 UA 明示 + polite delay のみを責務とする。

import { setTimeout as sleep } from 'node:timers/promises';

import type { SteamHttpOptions } from './types.js';

export const STEAM_USER_AGENT = 'LUDIARS-Canalis-Crawler/0.1 (+https://github.com/LUDIARS/Canalis)';

const MAX_REDIRECTS = 3;

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

  /**
   * @implements SPEC-CRAWL-STEAM — 転送は同じホスト内 (https) だけたどる。
   * steamcommunity は個人用 URL を設定した人の `/profiles/<SteamID64>/…` を `/id/<名前>/…` へ
   * 302 で送るため、転送を一律に拒否すると その人のレビューが取れない。 別ホストへの転送は拒否する。
   */
  const get = async (url: string): Promise<Response> => {
    let current = new URL(url);
    for (let hop = 0; ; hop++) {
      const res = await doFetch(current.toString(), {
        headers: { 'User-Agent': STEAM_USER_AGENT },
        redirect: 'manual',
      });
      if (res.status >= 300 && res.status < 400) {
        const location = res.headers.get('location');
        if (!location) throw new Error(`steam fetch HTTP ${res.status} without location`);
        const next = new URL(location, current);
        if (next.protocol !== 'https:' || next.host !== current.host) {
          throw new Error('steam fetch refused a redirect to another host');
        }
        if (hop >= MAX_REDIRECTS) throw new Error('steam fetch followed too many redirects');
        current = next;
        continue;
      }
      if (!res.ok) throw new Error(`steam fetch HTTP ${res.status}`);
      return res;
    }
  };

  return {
    getJson: async (url) => (await get(url)).json(),
    getText: async (url) => (await get(url)).text(),
    politeWait: () => wait(delayMs),
  };
}
