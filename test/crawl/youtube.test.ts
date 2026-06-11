import { describe, it, expect, vi } from 'vitest';
import { YouTubeSource } from '../../src/crawl/youtube/source.js';

const STUB_THREAD = {
  kind: 'youtube#commentThread',
  etag: 'etag1',
  id: 'thread1',
  snippet: {
    videoId: 'vid1',
    topLevelComment: {
      id: 'comment1',
      snippet: {
        videoId: 'vid1',
        textDisplay: 'Hello world',
        authorDisplayName: 'TestUser',
        authorChannelId: { value: 'chan1' },
        likeCount: 5,
        publishedAt: '2026-01-01T00:00:00Z',
        updatedAt: '2026-01-01T00:00:00Z',
      },
    },
    replyCount: 0,
    totalReplyCount: 0,
  },
};

function makeFetch(items: unknown[], nextPageToken?: string) {
  return vi.fn().mockResolvedValue({
    ok: true,
    json: async () => ({ items, nextPageToken }),
  });
}

describe('YouTubeSource', () => {
  it('returns RawRecord[] from comment threads', async () => {
    const fetch = makeFetch([STUB_THREAD]);
    const source = new YouTubeSource({ fetch: fetch as never, now: () => '2026-06-11T00:00:00Z' });
    const records = await source.crawl({ videoId: 'vid1', apiKey: 'key1' });

    expect(records).toHaveLength(1);
    const r = records[0];
    expect(r.source).toBe('youtube');
    expect(r.sourceId).toBe('thread1');
    expect(r.text).toBe('Hello world');
    expect(r.url).toBe('https://www.youtube.com/watch?v=vid1');
    expect(r.meta?.videoId).toBe('vid1');
    expect(r.meta?.likeCount).toBe(5);
  });

  it('paginates until maxResults is reached', async () => {
    const fetch = vi
      .fn()
      .mockResolvedValueOnce({
        ok: true,
        json: async () => ({ items: [STUB_THREAD], nextPageToken: 'next1' }),
      })
      .mockResolvedValueOnce({
        ok: true,
        json: async () => ({ items: [{ ...STUB_THREAD, id: 'thread2' }] }),
      });
    const source = new YouTubeSource({ fetch: fetch as never, now: () => '' });
    const records = await source.crawl({ videoId: 'vid1', apiKey: 'key1', maxResults: 200 });
    expect(records).toHaveLength(2);
    expect(fetch).toHaveBeenCalledTimes(2);
  });

  it('throws on missing videoId', async () => {
    const source = new YouTubeSource();
    await expect(source.crawl({ videoId: '', apiKey: 'k' })).rejects.toThrow('videoId');
  });

  it('throws on API error', async () => {
    const fetch = vi.fn().mockResolvedValue({ ok: false, status: 403, text: async () => 'forbidden' });
    const source = new YouTubeSource({ fetch: fetch as never, now: () => '' });
    await expect(source.crawl({ videoId: 'v', apiKey: 'k' })).rejects.toThrow('403');
  });
});
