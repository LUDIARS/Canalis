import { describe, it, expect, vi } from 'vitest';
import { RedditSource } from '../../src/crawl/reddit/source.js';

const STUB_POST = {
  id: 'post1',
  name: 't3_post1',
  title: 'Test Post',
  selftext: 'Hello from Reddit',
  url: 'https://i.redd.it/img.png',
  permalink: '/r/test/comments/post1/test_post/',
  author: 'user1',
  subreddit: 'test',
  score: 42,
  upvote_ratio: 0.95,
  num_comments: 10,
  created_utc: 1700000000,
};

const STUB_COMMENT = {
  id: 'cmt1',
  name: 't1_cmt1',
  body: 'Great post!',
  author: 'commenter1',
  permalink: '/r/test/comments/post1/test_post/cmt1/',
  subreddit: 'test',
  score: 5,
  created_utc: 1700001000,
  parent_id: 't3_post1',
  link_id: 't3_post1',
};

function makePostFetch(posts: unknown[]) {
  return vi.fn().mockResolvedValue({
    ok: true,
    json: async () => ({ data: { children: posts.map((p) => ({ data: p })) } }),
  });
}

function makeCommentFetch(comments: unknown[]) {
  return vi.fn().mockResolvedValue({
    ok: true,
    json: async () => [
      { data: { children: [{ data: STUB_POST }] } },
      { data: { children: comments.map((c) => ({ kind: 't1', data: c })) } },
    ],
  });
}

describe('RedditSource', () => {
  it('fetches posts from a subreddit', async () => {
    const fetch = makePostFetch([STUB_POST]);
    const source = new RedditSource({ fetch: fetch as never, now: () => '2026-06-11T00:00:00Z' });
    const records = await source.crawl({ subreddit: 'test', mode: 'posts' });

    expect(records).toHaveLength(1);
    const r = records[0];
    expect(r.source).toBe('reddit');
    expect(r.sourceId).toBe('t3_post1');
    expect(r.title).toBe('Test Post');
    expect(r.text).toBe('Hello from Reddit');
    expect(r.meta?.subreddit).toBe('test');
  });

  it('fetches comments from a post', async () => {
    const fetch = makeCommentFetch([STUB_COMMENT]);
    const source = new RedditSource({ fetch: fetch as never, now: () => '2026-06-11T00:00:00Z' });
    const records = await source.crawl({ postId: 'post1', subredditForPost: 'test', mode: 'comments' });

    expect(records).toHaveLength(1);
    const r = records[0];
    expect(r.source).toBe('reddit');
    expect(r.sourceId).toBe('t1_cmt1');
    expect(r.text).toBe('Great post!');
    expect(r.meta?.author).toBe('commenter1');
  });

  it('throws on missing subreddit for mode=posts', async () => {
    const source = new RedditSource();
    await expect(source.crawl({ mode: 'posts' })).rejects.toThrow('subreddit');
  });

  it('throws on missing postId for mode=comments', async () => {
    const source = new RedditSource();
    await expect(source.crawl({ mode: 'comments' })).rejects.toThrow('postId');
  });

  it('throws on API error', async () => {
    const fetch = vi.fn().mockResolvedValue({ ok: false, status: 429, text: async () => 'rate limited' });
    const source = new RedditSource({ fetch: fetch as never, now: () => '' });
    await expect(source.crawl({ subreddit: 'test' })).rejects.toThrow('429');
  });
});
