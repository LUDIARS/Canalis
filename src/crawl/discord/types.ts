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

/** Discord API の抽象 (テスト時に差し替え可能)。 */
export interface DiscordApi {
  getMessages(
    channelId: string,
    opts: { limit: number; before?: string; after?: string },
  ): Promise<DiscordMessage[]>;
}
