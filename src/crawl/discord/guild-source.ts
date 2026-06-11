// ①Crawl adapter: ギルド全体をクロールし、チャンネル/スレッド単位の RawRecord[] を返す。
// 出力は 1 チャンネル or フォーラム投稿につき 1 RawRecord。
// text フィールドに会話をタイムライン形式で格納する。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import type { DiscordGuildSourceConfig, DiscordApi, DiscordMessage, ChannelCrawlRecord } from './types.js';
import { createDiscordClient } from './client.js';
import { crawlGuild } from './guild.js';

export type DiscordGuildSourceDeps = {
  api?: DiscordApi;
  now?: () => string;
};

/** ギルド全チャンネル + フォーラムスレッドを対象にした ①Crawl adapter。 */
export class DiscordGuildSource implements Source<DiscordGuildSourceConfig> {
  readonly name = 'discord-guild';

  constructor(private readonly deps: DiscordGuildSourceDeps = {}) {}

  async crawl(config: DiscordGuildSourceConfig): Promise<RawRecord[]> {
    if (!config.guildId) throw new Error('DiscordGuildSource: guildId is required');

    const api = this.deps.api ?? createDiscordClient(config.token);
    const now = this.deps.now ?? (() => new Date().toISOString());

    const records = await crawlGuild(api, config.guildId, {
      channelTypes: config.channelTypes,
      excludeChannelIds: config.excludeChannelIds,
      messageOptions: config.options,
    });

    const fetchedAt = now();
    return records.map((r) => toRawRecord(r, config.guildId, fetchedAt));
  }
}

function toRawRecord(
  record: ChannelCrawlRecord,
  guildId: string,
  fetchedAt: string,
): RawRecord {
  return {
    source: 'discord-guild',
    sourceId: record.id,
    fetchedAt,
    url: `https://discord.com/channels/${guildId}/${record.id}`,
    title: record.forumChannelName
      ? `${record.forumChannelName} / ${record.name}`
      : record.name,
    text: messagesToText(record.messages),
    raw: {
      id: record.id,
      name: record.name,
      kind: record.kind,
      forumChannelName: record.forumChannelName,
      messages: record.messages,
    },
    meta: {
      guildId,
      channelId: record.id,
      channelName: record.name,
      kind: record.kind,
      forumChannelName: record.forumChannelName,
      messageCount: record.messages.length,
    },
  };
}

/** メッセージ配列を「[日時] 名前: 内容」形式のテキストに変換。 */
function messagesToText(messages: DiscordMessage[]): string {
  if (!messages.length) return '';
  return messages
    .slice()
    .sort((a, b) => a.timestamp.localeCompare(b.timestamp))
    .map((m) => {
      const ts = m.timestamp.slice(0, 16).replace('T', ' ') + ' UTC';
      const author = m.author.global_name ?? m.author.username;
      return `[${ts}] ${author}: ${m.content}`;
    })
    .join('\n');
}
