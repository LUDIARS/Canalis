// Discord チャンネルメッセージ取得クローラーの型定義。
// Discord REST API v10 を raw fetch で叩く (SDK 依存なし)。

/** Source<DiscordSourceConfig> に渡す設定。 */
export type DiscordSourceConfig = {
  /** 取得対象のチャンネル ID。 */
  channelId: string;
  /** Bot token。 省略時は env DISCORD_BOT_TOKEN。 */
  token?: string;
  /** ギルド ID (メッセージ URL 生成に使用、省略可)。 */
  guildId?: string;
  options?: DiscordCrawlOptions;
};

export type DiscordCrawlOptions = {
  /** 取得する最大メッセージ数。 デフォルト 100。 */
  maxMessages?: number;
  /** このメッセージ ID より古いメッセージを取得 (ページネーション用)。 */
  before?: string;
  /** このメッセージ ID より新しいメッセージを取得。 */
  after?: string;
  /** true = 古い順 (after カーソル使用)。 デフォルト false (新しい順 / before カーソル)。 */
  oldestFirst?: boolean;
};

/** Discord API から返る Message オブジェクトの使用フィールド。 */
export type DiscordMessage = {
  id: string;
  channel_id: string;
  author: {
    id: string;
    username: string;
    global_name: string | null;
  };
  content: string;
  timestamp: string;
  edited_timestamp: string | null;
  attachments: unknown[];
  embeds: unknown[];
  referenced_message?: DiscordMessage | null;
};

/** Source<DiscordGuildSourceConfig> に渡す設定。 */
export type DiscordGuildSourceConfig = {
  /** クロール対象のギルド (サーバー) ID。 */
  guildId: string;
  /** Bot token。 省略時は env DISCORD_BOT_TOKEN。 */
  token?: string;
  /**
   * 取得対象のチャンネルタイプ。
   * 0=GUILD_TEXT / 5=GUILD_ANNOUNCEMENT / 15=GUILD_FORUM。
   * デフォルト [0, 5, 15]。
   */
  channelTypes?: number[];
  /** 除外するチャンネル ID リスト。 */
  excludeChannelIds?: string[];
  /** チャンネル/スレッドごとに適用するクロールオプション。 */
  options?: DiscordCrawlOptions;
};

/** Discord Channel オブジェクトの使用フィールド。 */
export type DiscordChannel = {
  id: string;
  type: number;
  name: string;
  guild_id?: string;
};

/** Discord Thread (フォーラム投稿 / スレッド) の使用フィールド。 */
export type DiscordThread = {
  id: string;
  /** 11=PUBLIC_THREAD / 12=PRIVATE_THREAD */
  type: number;
  /** フォーラム投稿名またはスレッド名。 */
  name: string;
  /** 親チャンネル (フォーラムチャンネル) の ID。 */
  parent_id: string;
  guild_id?: string;
  thread_metadata?: {
    archived: boolean;
    archive_timestamp: string;
  };
};

/** ギルドクロールの 1 チャンネル/スレッド単位の中間結果。 */
export type ChannelCrawlRecord = {
  /** チャンネルまたはスレッドの ID (RawRecord.sourceId に使用)。 */
  id: string;
  /** チャンネル名またはフォーラム投稿名 (RawRecord.title に使用)。 */
  name: string;
  /** 'text-channel' | 'announcement' | 'forum-thread' */
  kind: string;
  /** フォーラムスレッドの場合、親フォーラムチャンネル名。 */
  forumChannelName?: string;
  messages: DiscordMessage[];
};

/** Discord API の抽象 (テスト時に差し替え可能)。 */
export interface DiscordApi {
  getMessages(
    channelId: string,
    opts: { limit: number; before?: string; after?: string },
  ): Promise<DiscordMessage[]>;
  getGuildChannels(guildId: string): Promise<DiscordChannel[]>;
  getActiveThreads(guildId: string): Promise<DiscordThread[]>;
  getArchivedThreads(channelId: string, before?: string): Promise<{ threads: DiscordThread[]; has_more: boolean }>;
}
