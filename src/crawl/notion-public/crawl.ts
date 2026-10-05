// 公開 Notion ページを起点に、 本文中のページリンクを maxDepth まで辿って取得する。
// token 不要 (ブラウザレンダリング)。 PageFetcher は注入可能 (テストで fake)。

import { PlaywrightFetcher } from './fetcher.js';
import type {
  NotionPublicCrawlResult,
  NotionPublicCrawledPage,
  NotionPublicPageCrawlOptions,
  PageFetcher,
} from './types.js';

type QueueItem = { url: string; depth: number; parentUrl: string };

/** 末尾スラッシュ / クエリ / ハッシュを落として同一ページを判定する。 */
function normalize(url: string): string {
  try {
    const u = new URL(url);
    return (u.origin + u.pathname).replace(/\/$/, '');
  } catch {
    return url;
  }
}

/**
 * 公開 Notion ページを起点 (depth 0) に幅優先で取得する。
 * fetcher 省略時は PlaywrightFetcher を作り、 終了時に close する。
 */
export async function crawlPublicPages(
  rootUrl: string,
  opts: NotionPublicPageCrawlOptions = {},
  fetcher?: PageFetcher,
): Promise<NotionPublicCrawlResult> {
  const maxDepth = opts.maxDepth ?? 0;
  const maxPages = opts.maxPages ?? 50;
  const fetchOpts = {
    timeout: opts.timeout ?? 30_000,
    scrollDelay: opts.scrollDelay ?? 800,
    maxScrolls: opts.maxScrolls ?? 15,
  };
  const owned = !fetcher;
  const f = fetcher ?? new PlaywrightFetcher();

  const pages: NotionPublicCrawledPage[] = [];
  const errors: NotionPublicCrawlResult['errors'] = [];
  const visited = new Set<string>();
  const queue: QueueItem[] = [{ url: rootUrl, depth: 0, parentUrl: '' }];
  let truncated = false;

  try {
    while (queue.length > 0) {
      if (pages.length >= maxPages) {
        truncated = true;
        break;
      }
      const item = queue.shift()!;
      const key = normalize(item.url);
      if (visited.has(key)) continue;
      visited.add(key);

      let fetched;
      try {
        fetched = await f.fetch(item.url, fetchOpts);
      } catch (err) {
        errors.push({ url: item.url, message: (err as Error).message });
        continue;
      }
      pages.push({
        url: item.url,
        title: fetched.title,
        depth: item.depth,
        parentUrl: item.parentUrl,
        markdown: fetched.markdown,
      });

      if (item.depth >= maxDepth) continue;
      for (const link of fetched.raw.links ?? []) {
        if (!visited.has(normalize(link))) {
          queue.push({ url: link, depth: item.depth + 1, parentUrl: item.url });
        }
      }
    }
  } finally {
    if (owned) await f.close?.();
  }

  return { rootUrl, pages, errors, truncated };
}
