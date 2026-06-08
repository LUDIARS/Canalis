// NotionSource が crawl 結果を RawRecord[] へ正しく変換することを、 fake NotionApi で検証する。
// (実 API へは繋がない。 ①→② 契約 = RawRecord の形を固定するテスト)

import { describe, it, expect } from 'vitest';
import { NotionSource } from '../src/crawl/notion/source.js';
import type { NotionApi, NotionBlock, NotionPage, Paged } from '../src/crawl/notion/types.js';

function paged<T>(results: T[]): Paged<T> {
  return { results, next_cursor: null, has_more: false };
}

/** 1 row + 1 paragraph だけ返す最小の fake。 */
class FakeNotionApi implements NotionApi {
  async queryDatabase(): Promise<Paged<NotionPage>> {
    const page: NotionPage = {
      id: 'page-1',
      url: 'https://notion.so/page-1',
      properties: {
        Name: { type: 'title', title: [{ plain_text: 'Acme Inc' }] },
        Tag: { type: 'multi_select', multi_select: [{ name: 'saas' }, { name: 'b2b' }] },
      },
    };
    return paged([page]);
  }

  async getBlockChildren(blockId: string): Promise<Paged<NotionBlock>> {
    if (blockId === 'page-1') {
      return paged<NotionBlock>([
        { id: 'b1', type: 'paragraph', paragraph: { rich_text: [{ plain_text: 'hello world' }] } },
      ]);
    }
    return paged<NotionBlock>([]);
  }

  async retrievePage(pageId: string): Promise<NotionPage> {
    return { id: pageId };
  }
}

describe('NotionSource', () => {
  it('crawl 結果を RawRecord[] に変換する', async () => {
    const source = new NotionSource({ api: new FakeNotionApi(), now: () => '2026-06-08T00:00:00.000Z' });
    const records = await source.crawl({ databaseId: 'db-1' });

    expect(records).toHaveLength(1);
    const r = records[0]!;
    expect(r.source).toBe('notion');
    expect(r.sourceId).toBe('page-1');
    expect(r.fetchedAt).toBe('2026-06-08T00:00:00.000Z');
    expect(r.title).toBe('Acme Inc');
    expect(r.text).toContain('hello world');
    // properties は meta 側に簡易 key→string で入る
    expect((r.meta as { properties: Record<string, string> }).properties['Tag']).toBe('saas, b2b');
    // raw は verbatim (replay 用) に保持される
    expect(r.raw).toBeTruthy();
  });

  it('databaseId 未指定は fail-fast する', async () => {
    const source = new NotionSource({ api: new FakeNotionApi() });
    await expect(source.crawl({ databaseId: '' })).rejects.toThrow(/databaseId/);
  });
});
