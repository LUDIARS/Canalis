// ①Crawl adapter: 公開 Notion ページをブラウザレンダリングで取得し RawRecord[] へ変換する。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import type { NotionPublicSourceConfig, PageFetcher } from './types.js';
import { PlaywrightFetcher } from './fetcher.js';

export type NotionPublicSourceDeps = {
  /** テスト時に差し替える PageFetcher。 省略時は PlaywrightFetcher。 */
  fetcher?: PageFetcher;
  /** fetchedAt の時刻源 (テスト決定性のため注入可能)。 */
  now?: () => string;
};

/** 公開 Notion ページを 1 source として扱う ①Crawl adapter。 */
export class NotionPublicSource implements Source<NotionPublicSourceConfig> {
  readonly name = 'notion-public';

  constructor(private readonly deps: NotionPublicSourceDeps = {}) {}

  async crawl(config: NotionPublicSourceConfig): Promise<RawRecord[]> {
    if (!config.url) throw new Error('NotionPublicSource: url is required');

    const fetcher = this.deps.fetcher ?? new PlaywrightFetcher();
    const now = this.deps.now ?? (() => new Date().toISOString());

    const opts = {
      timeout: config.options?.timeout ?? 30_000,
      scrollDelay: config.options?.scrollDelay ?? 800,
      maxScrolls: config.options?.maxScrolls ?? 15,
    };

    const page = await fetcher.fetch(config.url, opts);
    const fetchedAt = now();

    return [
      {
        source: 'notion-public',
        sourceId: config.url,
        fetchedAt,
        url: config.url,
        title: page.title || undefined,
        text: page.markdown || undefined,
        raw: page.raw,
        meta: {
          url: config.url,
          blockCount: page.raw.blocks.length,
        },
      },
    ];
  }
}
