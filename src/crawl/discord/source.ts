// ①Crawl adapter: Discord チャンネルのメッセージを RawRecord[] へ変換する。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import type { DiscordSourceConfig, DiscordApi } from './types.js';
import { createDiscordClient } from './client.js';
import { crawlChannel } from './crawl.js';

export type DiscordSourceDeps = {
  /** テスト時に差し替える DiscordApi。 省略時は config.token / env から生成。 */
  api?: DiscordApi;
  /** fetchedAt の時刻源 (テスト決定性のため注入可能)。 */
  now?: () => string;
};

/** Discord チャンネルを 1 source として扱う ①Crawl adapter。 */
export class DiscordSource implements Source<DiscordSourceConfig> {
  readonly name = 'discord';

  constructor(private readonly deps: DiscordSourceDeps = {}) {}

  async crawl(config: DiscordSourceConfig): Promise<RawRecord[]> {
    if (!config.channelId) throw new Error('DiscordSource: channelId is required');

    const api = this.deps.api ?? createDiscordClient(config.token);
    const now = this.deps.now ?? (() => new Date().toISOString());

    const result = await crawlChannel(api, config.channelId, config.options);
    const fetchedAt = now();

    return result.messages.map((msg) => ({
      source: 'discord',
      sourceId: msg.id,
      fetchedAt,
      url: buildMessageUrl(msg, config.guildId),
      title: undefined,
      text: msg.content || undefined,
      raw: msg,
      meta: {
        channelId: config.channelId,
        authorId: msg.author.id,
        authorName: msg.author.global_name ?? msg.author.username,
        timestamp: msg.timestamp,
        editedTimestamp: msg.edited_timestamp ?? undefined,
        hasAttachments: msg.attachments.length > 0,
        hasEmbeds: msg.embeds.length > 0,
        isReply: msg.referenced_message != null,
        truncated: result.truncated,
      },
    }));
  }
}

function buildMessageUrl(
  msg: { id: string; channel_id: string },
  guildId?: string,
): string {
  const guild = guildId ?? '@me';
  return `https://discord.com/channels/${guild}/${msg.channel_id}/${msg.id}`;
}
