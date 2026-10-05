// crawlPublicPages (公開 Notion のリンク辿り) を fake PageFetcher で検証する (ブラウザは起動しない)。

import { describe, it, expect } from 'vitest';
import { crawlPublicPages } from '../src/crawl/notion-public/crawl.js';
import { isNotionUrl, parseNotionPageId } from '../src/crawl/notion/url.js';
import type { FetchedPage, PageFetcher } from '../src/crawl/notion-public/types.js';

const ROOT = 'https://acme.notion.site/Root-00000000000000000000000000000000';
const C1 = 'https://acme.notion.site/C1-11111111111111111111111111111111';
const C2 = 'https://acme.notion.site/C2-22222222222222222222222222222222';
const C3 = 'https://acme.notion.site/C3-33333333333333333333333333333333';

/** ROOT → C1 → C2 → C3 の連鎖。 C1 は ROOT へ戻るリンクも持つ (循環)。 */
class ChainFetcher implements PageFetcher {
  readonly fetched: string[] = [];
  closed = 0;
  private readonly links: Record<string, string[]> = {
    [ROOT]: [C1],
    [C1]: [ROOT, C2],
    [C2]: [C3],
    [C3]: [],
  };

  async fetch(url: string): Promise<FetchedPage> {
    this.fetched.push(url);
    if (!(url in this.links)) throw new Error(`404 ${url}`);
    const title = url.split('/').pop()!.split('-')[0]!;
    return {
      url,
      title,
      markdown: `${title} body`,
      raw: { title, blocks: [{ type: 'text', text: `${title} body` }], links: this.links[url] },
    };
  }

  async close(): Promise<void> {
    this.closed++;
  }
}

describe('crawlPublicPages', () => {
  it('既定 (maxDepth 0) は起点ページのみ', async () => {
    const f = new ChainFetcher();
    const r = await crawlPublicPages(ROOT, {}, f);
    expect(r.pages.map((p) => p.title)).toEqual(['Root']);
  });

  it('maxDepth まで幅優先で辿り、 循環リンクは 1 回だけ取る', async () => {
    const f = new ChainFetcher();
    const r = await crawlPublicPages(ROOT, { maxDepth: 2 }, f);
    expect(r.pages.map((p) => [p.title, p.depth, p.parentUrl])).toEqual([
      ['Root', 0, ''],
      ['C1', 1, ROOT],
      ['C2', 2, C1],
    ]);
    expect(f.fetched).toEqual([ROOT, C1, C2]);
    expect(r.truncated).toBe(false);
  });

  it('maxPages で打ち切ると truncated=true', async () => {
    const r = await crawlPublicPages(ROOT, { maxDepth: 5, maxPages: 2 }, new ChainFetcher());
    expect(r.pages).toHaveLength(2);
    expect(r.truncated).toBe(true);
  });

  it('取得失敗は errors に積んで続行する', async () => {
    const f = new ChainFetcher();
    const r = await crawlPublicPages('https://acme.notion.site/Missing-44444444444444444444444444444444', {}, f);
    expect(r.pages).toEqual([]);
    expect(r.errors[0]!.message).toMatch(/404/);
  });

  it('注入した fetcher は呼び出し側の所有なので close しない', async () => {
    const f = new ChainFetcher();
    await crawlPublicPages(ROOT, { maxDepth: 1 }, f);
    expect(f.closed).toBe(0);
  });
});

describe('Notion URL (notion.com)', () => {
  it('app.notion.com/p/<id> を Notion URL として読む', () => {
    const url = 'https://app.notion.com/p/2c439cbfbab98011b008e3262ce3a98b?pvs=204';
    expect(isNotionUrl(url)).toBe(true);
    expect(parseNotionPageId(url)).toBe('2c439cbf-bab9-8011-b008-e3262ce3a98b');
  });
});
