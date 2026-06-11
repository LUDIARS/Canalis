import { describe, it, expect } from 'vitest';
import { NotionPublicSource } from '../src/crawl/notion-public/source.js';
import { blocksToMarkdown } from '../src/crawl/notion-public/extract.js';
import type { PageFetcher } from '../src/crawl/notion-public/types.js';

const FIXED_NOW = '2026-06-11T00:00:00.000Z';
const TEST_URL = 'https://notion.so/test-abc123';

const fakeFetcher: PageFetcher = {
  async fetch(url) {
    return {
      url,
      title: 'Test Page',
      markdown: '# Test Page\n\nSome content here.',
      raw: {
        title: 'Test Page',
        blocks: [
          { type: 'h1', text: 'Test Page' },
          { type: 'text', text: 'Some content here.' },
        ],
      },
    };
  },
};

describe('NotionPublicSource', () => {
  it('crawl returns RawRecord with correct shape', async () => {
    const src = new NotionPublicSource({ fetcher: fakeFetcher, now: () => FIXED_NOW });
    const records = await src.crawl({ url: TEST_URL });

    expect(records).toHaveLength(1);
    const r = records[0];
    expect(r.source).toBe('notion-public');
    expect(r.sourceId).toBe(TEST_URL);
    expect(r.fetchedAt).toBe(FIXED_NOW);
    expect(r.url).toBe(TEST_URL);
    expect(r.title).toBe('Test Page');
    expect(r.text).toBe('# Test Page\n\nSome content here.');
    expect(r.meta?.blockCount).toBe(2);
    expect(r.raw).toBeDefined();
  });

  it('throws if url is empty', async () => {
    const src = new NotionPublicSource({ fetcher: fakeFetcher });
    await expect(src.crawl({ url: '' })).rejects.toThrow('url is required');
  });
});

describe('blocksToMarkdown', () => {
  it('converts heading blocks', () => {
    expect(blocksToMarkdown([{ type: 'h1', text: 'Title' }])).toBe('# Title');
    expect(blocksToMarkdown([{ type: 'h2', text: 'Sub' }])).toBe('## Sub');
    expect(blocksToMarkdown([{ type: 'h3', text: 'Sub2' }])).toBe('### Sub2');
  });

  it('converts list blocks', () => {
    expect(blocksToMarkdown([{ type: 'li', text: 'Item' }])).toBe('- Item');
    expect(blocksToMarkdown([{ type: 'oli', text: 'Item' }])).toBe('1. Item');
  });

  it('converts quote and code blocks', () => {
    expect(blocksToMarkdown([{ type: 'quote', text: 'cited' }])).toBe('> cited');
    expect(blocksToMarkdown([{ type: 'code', text: 'x = 1' }])).toBe('```\nx = 1\n```');
  });

  it('converts divider', () => {
    expect(blocksToMarkdown([{ type: 'divider', text: '' }])).toBe('---');
  });

  it('joins multiple blocks with double newline', () => {
    const result = blocksToMarkdown([
      { type: 'h1', text: 'Title' },
      { type: 'text', text: 'Body text.' },
    ]);
    expect(result).toBe('# Title\n\nBody text.');
  });
});
