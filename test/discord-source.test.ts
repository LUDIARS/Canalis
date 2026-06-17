import { describe, it, expect } from 'vitest';
import { DiscordSource } from '../src/crawl/discord/source.js';
import { crawlChannel } from '../src/crawl/discord/crawl.js';
import type { DiscordApi, DiscordMessage } from '../src/crawl/discord/types.js';

const FIXED_NOW = '2026-06-11T00:00:00.000Z';
const CHANNEL_ID = '123456789012345678';
const GUILD_ID = '987654321098765432';

function makeMessage(id: string, content: string, extra?: Partial<DiscordMessage>): DiscordMessage {
  return {
    id,
    channel_id: CHANNEL_ID,
    author: { id: '111', username: 'testuser', global_name: 'Test User' },
    content,
    timestamp: '2026-06-11T00:00:00.000Z',
    edited_timestamp: null,
    attachments: [],
    embeds: [],
    ...extra,
  };
}

const fakeApi: DiscordApi = {
  async getMessages(_channelId, opts) {
    const all = [
      makeMessage('300', 'newest message'),
      makeMessage('200', 'middle message'),
      makeMessage('100', 'oldest message'),
    ];
    if (opts.before) {
      const idx = all.findIndex((m) => m.id === opts.before);
      return all.slice(idx + 1, idx + 1 + opts.limit);
    }
    if (opts.after) {
      const idx = all.findIndex((m) => m.id === opts.after);
      return all.slice(Math.max(0, idx - opts.limit), idx).reverse();
    }
    return all.slice(0, opts.limit);
  },
};

describe('DiscordSource', () => {
  it('crawl returns RawRecord with correct shape', async () => {
    const src = new DiscordSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ channelId: CHANNEL_ID, guildId: GUILD_ID });

    expect(records.length).toBeGreaterThan(0);
    const r = records[0];
    expect(r.source).toBe('discord');
    expect(r.sourceId).toBe('300');
    expect(r.fetchedAt).toBe(FIXED_NOW);
    expect(r.url).toBe(`https://discord.com/channels/${GUILD_ID}/${CHANNEL_ID}/300`);
    expect(r.text).toBe('newest message');
    expect(r.meta?.channelId).toBe(CHANNEL_ID);
    expect(r.meta?.authorName).toBe('Test User');
  });

  it('url uses @me when guildId is omitted', async () => {
    const src = new DiscordSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ channelId: CHANNEL_ID });
    expect(records[0].url).toContain('@me');
  });

  it('throws if channelId is empty', async () => {
    const src = new DiscordSource({ api: fakeApi });
    await expect(src.crawl({ channelId: '' })).rejects.toThrow('channelId is required');
  });

  it('respects maxMessages option', async () => {
    const src = new DiscordSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ channelId: CHANNEL_ID, options: { maxMessages: 1 } });
    expect(records).toHaveLength(1);
  });
});

describe('crawlChannel', () => {
  it('returns messages and truncated flag', async () => {
    const result = await crawlChannel(fakeApi, CHANNEL_ID, { maxMessages: 1 });
    expect(result.messages).toHaveLength(1);
    expect(result.truncated).toBe(true);
  });

  it('truncated=false when all messages fetched', async () => {
    const result = await crawlChannel(fakeApi, CHANNEL_ID, { maxMessages: 100 });
    expect(result.truncated).toBe(false);
  });
});
