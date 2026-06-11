// Discord チャンネルのメッセージをページネーションで取得するコア関数。

import type { DiscordApi, DiscordMessage, DiscordCrawlOptions } from './types.js';

const PAGE_SIZE = 100; // Discord API の 1 リクエスト上限

export type ChannelCrawlResult = {
  messages: DiscordMessage[];
  /** maxMessages に達して打ち切った場合 true。 */
  truncated: boolean;
};

/**
 * channelId のメッセージを options に従って取得する。
 * デフォルトは新しい順 (before カーソル)。 oldestFirst=true で古い順 (after カーソル)。
 */
export async function crawlChannel(
  api: DiscordApi,
  channelId: string,
  options: DiscordCrawlOptions = {},
): Promise<ChannelCrawlResult> {
  const maxMessages = options.maxMessages ?? 100;
  const oldestFirst = options.oldestFirst ?? false;

  const messages: DiscordMessage[] = [];
  let cursor: string | undefined = oldestFirst ? options.after : options.before;
  let truncated = false;

  while (messages.length < maxMessages) {
    const remaining = maxMessages - messages.length;
    const limit = Math.min(PAGE_SIZE, remaining);

    const params: { limit: number; before?: string; after?: string } = { limit };
    if (oldestFirst) {
      if (cursor) params.after = cursor;
    } else {
      if (cursor) params.before = cursor;
    }

    const batch = await api.getMessages(channelId, params);
    if (!batch.length) break;

    messages.push(...batch);

    if (messages.length >= maxMessages) {
      truncated = true;
      break;
    }

    // 次ページカーソルを更新
    if (oldestFirst) {
      // after 方向: 最新のメッセージ ID (末尾) を次の after に
      cursor = batch[batch.length - 1]?.id;
    } else {
      // before 方向: 最古のメッセージ ID (末尾) を次の before に
      cursor = batch[batch.length - 1]?.id;
    }

    // Discord API は100件未満を返したら末尾
    if (batch.length < PAGE_SIZE) break;
  }

  return { messages, truncated };
}
