import { describe, it, expect } from 'vitest';
import { DiscordGuildSource } from '../src/crawl/discord/guild-source.js';
import { crawlGuild } from '../src/crawl/discord/guild.js';
import type { DiscordApi, DiscordChannel, DiscordMessage, DiscordThread } from '../src/crawl/discord/types.js';

const FIXED_NOW = '2026-06-11T00:00:00.000Z';
const GUILD_ID = '987654321098765432';

function makeMsg(id: string, channelId: string, content: string): DiscordMessage {
  return {
    id,
    channel_id: channelId,
    author: { id: '111', username: 'user1', global_name: 'User One' },
    content,
    timestamp: `2026-06-11T0${id}:00:00.000Z`,
    edited_timestamp: null,
    attachments: [],
    embeds: [],
  };
}

const TEXT_CHANNEL: DiscordChannel = { id: 'ch-text', type: 0, name: 'general', guild_id: GUILD_ID };
const FORUM_CHANNEL: DiscordChannel = { id: 'ch-forum', type: 15, name: 'announcements', guild_id: GUILD_ID };
const FORUM_THREAD: DiscordThread = {
  id: 'thread-1',
  type: 11,
  name: 'Release v1.0',
  parent_id: 'ch-forum',
  guild_id: GUILD_ID,
};

const fakeApi: DiscordApi = {
  async getGuildChannels() { return [TEXT_CHANNEL, FORUM_CHANNEL]; },
  async getActiveThreads() { return [FORUM_THREAD]; },
  async getArchivedThreads() { return { threads: [], has_more: false }; },
  async getMessages(channelId, opts) {
    const msgMap: Record<string, DiscordMessage[]> = {
      'ch-text': [makeMsg('1', 'ch-text', 'hello from general')],
      'thread-1': [makeMsg('2', 'thread-1', 'release notes here')],
    };
    return (msgMap[channelId] ?? []).slice(0, opts.limit);
  },
};

describe('DiscordGuildSource', () => {
  it('returns one RawRecord per channel and forum thread', async () => {
    const src = new DiscordGuildSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ guildId: GUILD_ID });

    expect(records).toHaveLength(2);
    expect(records.map((r) => r.source)).toEqual(['discord-guild', 'discord-guild']);
  });

  it('text-channel record has correct title and text', async () => {
    const src = new DiscordGuildSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ guildId: GUILD_ID });

    const textRec = records.find((r) => r.meta?.kind === 'text-channel');
    expect(textRec?.title).toBe('general');
    expect(textRec?.text).toContain('hello from general');
    expect(textRec?.text).toContain('User One:');
  });

  it('forum-thread record includes forum channel name in title', async () => {
    const src = new DiscordGuildSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ guildId: GUILD_ID });

    const forumRec = records.find((r) => r.meta?.kind === 'forum-thread');
    expect(forumRec?.title).toBe('announcements / Release v1.0');
    expect(forumRec?.text).toContain('release notes here');
    expect(forumRec?.meta?.forumChannelName).toBe('announcements');
  });

  it('throws if guildId is empty', async () => {
    const src = new DiscordGuildSource({ api: fakeApi });
    await expect(src.crawl({ guildId: '' })).rejects.toThrow('guildId is required');
  });

  it('respects excludeChannelIds', async () => {
    const src = new DiscordGuildSource({ api: fakeApi, now: () => FIXED_NOW });
    const records = await src.crawl({ guildId: GUILD_ID, excludeChannelIds: ['ch-text'] });
    expect(records.every((r) => r.meta?.channelId !== 'ch-text')).toBe(true);
  });
});

describe('crawlGuild', () => {
  it('returns ChannelCrawlRecord for text channel and forum thread', async () => {
    const results = await crawlGuild(fakeApi, GUILD_ID);
    expect(results).toHaveLength(2);
    expect(results.map((r) => r.kind).sort()).toEqual(['forum-thread', 'text-channel']);
  });
});
