// Discord REST API v10 クライアント。
// raw fetch のみ使用 (外部 SDK 依存なし)。

import type { DiscordApi, DiscordMessage } from './types.js';

const BASE_URL = 'https://discord.com/api/v10';

export type DiscordClientOptions = {
  token: string;
};

export class DiscordApiClient implements DiscordApi {
  constructor(private readonly opts: DiscordClientOptions) {}

  async getMessages(
    channelId: string,
    params: { limit: number; before?: string; after?: string },
  ): Promise<DiscordMessage[]> {
    const query = new URLSearchParams({ limit: String(params.limit) });
    if (params.before) query.set('before', params.before);
    if (params.after) query.set('after', params.after);

    return this.request<DiscordMessage[]>(
      `${BASE_URL}/channels/${channelId}/messages?${query}`,
    );
  }

  private async request<T>(url: string, attempt = 0): Promise<T> {
    const res = await fetch(url, {
      headers: { Authorization: `Bot ${this.opts.token}` },
    });

    if (res.status === 429) {
      // Rate limited — Retry-After は秒単位の float
      const retryAfter = parseFloat(res.headers.get('Retry-After') ?? '1');
      if (attempt >= 3) throw new Error(`DiscordApi: rate limit retries exhausted (${url})`);
      await sleep(Math.ceil(retryAfter * 1000) + 100);
      return this.request<T>(url, attempt + 1);
    }

    if (!res.ok) {
      const body = await res.text().catch(() => '');
      throw new Error(`DiscordApi: ${res.status} ${res.statusText} — ${url}\n${body}`);
    }

    return res.json() as Promise<T>;
  }
}

export function createDiscordClient(token?: string): DiscordApi {
  const t = token ?? process.env['DISCORD_BOT_TOKEN'];
  if (!t) throw new Error('DiscordApiClient: token is required (config.token or env DISCORD_BOT_TOKEN)');
  return new DiscordApiClient({ token: t });
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
