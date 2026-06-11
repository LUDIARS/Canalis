// ギルド (サーバー) 全体のクロール。
// テキストチャンネル + フォーラムスレッドを対象に、
// チャンネル/スレッド単位で ChannelCrawlRecord[] を返す。

import type { DiscordApi, DiscordCrawlOptions, DiscordThread, ChannelCrawlRecord } from './types.js';
import { crawlChannel } from './crawl.js';

/** GUILD_TEXT=0 / GUILD_ANNOUNCEMENT=5 / GUILD_FORUM=15 */
export const DEFAULT_CHANNEL_TYPES = [0, 5, 15];

export type GuildCrawlOptions = {
  channelTypes?: number[];
  excludeChannelIds?: string[];
  messageOptions?: DiscordCrawlOptions;
};

/** ギルド全チャンネル + フォーラムスレッドをクロールし、チャンネル単位のレコードを返す。 */
export async function crawlGuild(
  api: DiscordApi,
  guildId: string,
  opts: GuildCrawlOptions = {},
): Promise<ChannelCrawlRecord[]> {
  const targetTypes = opts.channelTypes ?? DEFAULT_CHANNEL_TYPES;
  const excludeSet = new Set(opts.excludeChannelIds ?? []);

  const allChannels = await api.getGuildChannels(guildId);
  const channels = allChannels.filter(
    (ch) => targetTypes.includes(ch.type) && !excludeSet.has(ch.id),
  );

  const records: ChannelCrawlRecord[] = [];

  for (const ch of channels) {
    if (ch.type === 15) {
      // GUILD_FORUM: スレッドを全取得してそれぞれクロール
      const threads = await collectForumThreads(api, guildId, ch.id);
      for (const thread of threads) {
        if (excludeSet.has(thread.id)) continue;
        const result = await crawlChannel(api, thread.id, opts.messageOptions);
        records.push({
          id: thread.id,
          name: thread.name,
          kind: 'forum-thread',
          forumChannelName: ch.name,
          messages: result.messages,
        });
      }
    } else {
      // GUILD_TEXT / GUILD_ANNOUNCEMENT
      const kind = ch.type === 5 ? 'announcement' : 'text-channel';
      const result = await crawlChannel(api, ch.id, opts.messageOptions);
      records.push({
        id: ch.id,
        name: ch.name,
        kind,
        messages: result.messages,
      });
    }
  }

  return records;
}

/** フォーラムチャンネルの全スレッド (アクティブ + アーカイブ済み) を収集する。 */
async function collectForumThreads(
  api: DiscordApi,
  guildId: string,
  forumChannelId: string,
): Promise<DiscordThread[]> {
  const seen = new Set<string>();
  const threads: DiscordThread[] = [];

  // アクティブスレッド (ギルド全体から取得して対象フォーラムでフィルタ)
  const active = await api.getActiveThreads(guildId);
  for (const t of active) {
    if (t.parent_id === forumChannelId && !seen.has(t.id)) {
      seen.add(t.id);
      threads.push(t);
    }
  }

  // アーカイブ済みスレッド (ページネーション)
  let before: string | undefined;
  for (;;) {
    const { threads: archived, has_more } = await api.getArchivedThreads(forumChannelId, before);
    for (const t of archived) {
      if (!seen.has(t.id)) {
        seen.add(t.id);
        threads.push(t);
      }
    }
    if (!has_more || !archived.length) break;
    before = archived[archived.length - 1]?.thread_metadata?.archive_timestamp;
    if (!before) break;
  }

  return threads;
}
