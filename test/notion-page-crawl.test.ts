// crawlPage (ページ起点クロール) の深さ制御と、 Notion URL → page id 変換を fake NotionApi で検証する。

import { describe, it, expect } from 'vitest';
import { crawlPage } from '../src/crawl/notion/crawl.js';
import { findNotionUrls, isNotionUrl, parseNotionPageId } from '../src/crawl/notion/url.js';
import type { NotionApi, NotionBlock, NotionPage, Paged } from '../src/crawl/notion/types.js';

function paged<T>(results: T[]): Paged<T> {
  return { results, next_cursor: null, has_more: false };
}

function para(id: string, text: string): NotionBlock {
  return { id, type: 'paragraph', paragraph: { rich_text: [{ plain_text: text }] } };
}

function childPage(id: string, title: string): NotionBlock {
  return { id, type: 'child_page', child_page: { title } };
}

/** root → c1 → c2 → c3 の 3 段の子ページを持つ fake。 */
class ChainNotionApi implements NotionApi {
  private readonly tree: Record<string, NotionBlock[]> = {
    root: [para('p0', 'root body'), childPage('c1', 'C1')],
    c1: [para('p1', 'c1 body'), childPage('c2', 'C2')],
    c2: [para('p2', 'c2 body'), childPage('c3', 'C3')],
    c3: [para('p3', 'c3 body')],
  };

  async queryDatabase(): Promise<Paged<NotionPage>> {
    return paged([]);
  }

  async getBlockChildren(blockId: string): Promise<Paged<NotionBlock>> {
    return paged(this.tree[blockId] ?? []);
  }

  async retrievePage(pageId: string): Promise<NotionPage> {
    return {
      id: pageId,
      url: `https://www.notion.so/${pageId}`,
      properties: { title: { type: 'title', title: [{ plain_text: `T-${pageId}` }] } },
    };
  }
}

describe('crawlPage', () => {
  it('起点ページを depth 0 / kind=page で返し、 maxDepth まで子ページを辿る', async () => {
    const r = await crawlPage(new ChainNotionApi(), 'root', { maxDepth: 2 });
    expect(r.pageId).toBe('root');
    expect(r.pages.map((p) => [p.id, p.depth, p.kind])).toEqual([
      ['root', 0, 'page'],
      ['c1', 1, 'child_page'],
      ['c2', 2, 'child_page'],
    ]);
    expect(r.pages[0]!.title).toBe('T-root');
    expect(r.pages[0]!.markdown).toContain('root body');
    expect(r.errors).toEqual([]);
    expect(r.truncated).toBe(false);
  });

  it('maxDepth=0 なら起点ページ本文のみ', async () => {
    const r = await crawlPage(new ChainNotionApi(), 'root', { maxDepth: 0 });
    expect(r.pages.map((p) => p.id)).toEqual(['root']);
  });

  it('maxPages で打ち切ると truncated=true', async () => {
    const r = await crawlPage(new ChainNotionApi(), 'root', { maxDepth: 5, maxPages: 2 });
    expect(r.pages).toHaveLength(2);
    expect(r.truncated).toBe(true);
  });

  it('retrievePage 失敗は errors に積んで落ちない', async () => {
    const api = new ChainNotionApi();
    api.retrievePage = async () => {
      throw new Error('404');
    };
    const r = await crawlPage(api, 'root');
    expect(r.pages).toEqual([]);
    expect(r.errors[0]).toMatchObject({ id: 'root', stage: 'retrievePage' });
  });
});

describe('Notion URL', () => {
  const hex = '0123456789abcdef0123456789abcdef';
  const uuid = '01234567-89ab-cdef-0123-456789abcdef';

  it('タイトル付き path の末尾 32hex を UUID にする', () => {
    expect(parseNotionPageId(`https://www.notion.so/ws/Some-Title-${hex}`)).toBe(uuid);
    expect(parseNotionPageId(`https://acme.notion.site/Page-${hex}?pvs=4`)).toBe(uuid);
  });

  it('?p= (peek) を path より優先する', () => {
    const other = 'ffffffffffffffffffffffffffffffff';
    expect(parseNotionPageId(`https://www.notion.so/${other}?v=1&p=${hex}`)).toBe(uuid);
  });

  it('UUID 形式の path も読む', () => {
    expect(parseNotionPageId(`https://www.notion.so/${uuid}`)).toBe(uuid);
  });

  it('Notion 以外 / id 無しは null', () => {
    expect(parseNotionPageId(`https://example.com/${hex}`)).toBeNull();
    expect(parseNotionPageId('https://www.notion.so/ws')).toBeNull();
    expect(isNotionUrl('https://evilnotion.so/x')).toBe(false);
  });

  it('本文中の Notion URL を重複なしで列挙し、 末尾の句読点を外す', () => {
    const text = `仕様は https://www.notion.so/a-${hex}。 参考 https://example.com/x と https://www.notion.so/a-${hex}`;
    expect(findNotionUrls(text)).toEqual([`https://www.notion.so/a-${hex}`]);
  });
});
