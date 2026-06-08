// ①Crawl adapter: Notion DB クロール結果を Canalis の RawRecord[] へ変換する。
// crawlDatabase (移植本体) を Source 契約に適合させる薄いラッパ。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import { NotionApiClient } from './client.js';
import { crawlDatabase } from './crawl.js';
import type { CrawlOptions, NotionApi } from './types.js';

export type NotionSourceConfig = {
  /** クロール対象の Notion database id。 */
  databaseId: string;
  /** integration token。 省略時は env NOTION_TOKEN。 */
  token?: string;
  /** クロール深さ等のオプション。 */
  crawl?: CrawlOptions;
};

export type NotionSourceDeps = {
  /** テスト/再利用のために NotionApi を注入可能。 省略時は config.token / env から生成。 */
  api?: NotionApi;
  /** fetchedAt の時刻源 (テスト決定性のため注入可能)。 */
  now?: () => string;
};

/** Notion DB を 1 source として扱う ①Crawl adapter。 */
export class NotionSource implements Source<NotionSourceConfig> {
  readonly name = 'notion';

  constructor(private readonly deps: NotionSourceDeps = {}) {}

  async crawl(config: NotionSourceConfig): Promise<RawRecord[]> {
    if (!config.databaseId) throw new Error('NotionSource: databaseId is required');
    const api = this.deps.api ?? this.buildApi(config);
    const now = this.deps.now ?? (() => new Date().toISOString());

    const result = await crawlDatabase(api, config.databaseId, config.crawl);
    const fetchedAt = now();

    return result.pages.map((p) => ({
      source: 'notion',
      sourceId: p.id,
      fetchedAt,
      url: p.url || undefined,
      title: p.title || undefined,
      text: p.markdown || undefined,
      raw: p, // クロール結果 (properties 含む) を verbatim 保持
      meta: {
        kind: p.kind,
        parentId: p.parentId,
        depth: p.depth,
        databaseId: config.databaseId,
        properties: p.properties,
        truncated: result.truncated,
      },
    }));
  }

  /** config.token / env から NotionApiClient を生成 (fail-fast)。 */
  private buildApi(config: NotionSourceConfig): NotionApi {
    const token = config.token ?? process.env['NOTION_TOKEN'];
    if (!token) {
      throw new Error('NotionSource: token is required (config.token or env NOTION_TOKEN)');
    }
    return new NotionApiClient({ token, notionVersion: process.env['NOTION_VERSION'] });
  }
}
