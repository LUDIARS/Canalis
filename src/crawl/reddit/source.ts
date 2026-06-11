// ①Crawl adapter: Reddit 公開 JSON API でポスト/コメントを取得し RawRecord[] へ変換する。
// 認証不要 (公開 subreddit / 投稿)。 User-Agent は必須 (Reddit 規約)。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import type { RedditSourceConfig, RedditPost, RedditComment } from './types.js';

type CommentChild = {
  kind: string;
  data: RedditComment & { replies?: { data: { children: CommentChild[] } } };
};

const USER_AGENT = 'Canalis-Crawler/1.0 (LUDIARS; +https://github.com/LUDIARS)';
const BASE = 'https://www.reddit.com';

export type RedditSourceDeps = {
  fetch?: typeof globalThis.fetch;
  now?: () => string;
};

/** Reddit 投稿/コメントを 1 source として扱う ①Crawl adapter。 */
export class RedditSource implements Source<RedditSourceConfig> {
  readonly name = 'reddit';

  constructor(private readonly deps: RedditSourceDeps = {}) {}

  async crawl(config: RedditSourceConfig): Promise<RawRecord[]> {
    const mode = config.mode ?? 'posts';
    const fetcher = this.deps.fetch ?? globalThis.fetch;
    const now = this.deps.now ?? (() => new Date().toISOString());
    const fetchedAt = now();

    if (mode === 'comments') {
      if (!config.postId) throw new Error('RedditSource: postId is required for mode=comments');
      const sub = config.subredditForPost ?? 'unknown';
      return this.fetchComments(fetcher, sub, config.postId, config.limit ?? 500, fetchedAt);
    }

    if (!config.subreddit) throw new Error('RedditSource: subreddit is required for mode=posts');
    return this.fetchPosts(fetcher, config.subreddit, config.sort ?? 'hot', config.limit ?? 100, fetchedAt);
  }

  private async fetchPosts(
    fetcher: typeof globalThis.fetch,
    subreddit: string,
    sort: string,
    limit: number,
    fetchedAt: string,
  ): Promise<RawRecord[]> {
    const url = `${BASE}/r/${subreddit}/${sort}.json?limit=${limit}&raw_json=1`;
    const res = await this.get(fetcher, url) as { data?: { children?: Array<{ data: RedditPost }> } };
    const posts = res.data?.children ?? [];

    return posts.map(({ data: p }) => ({
      source: 'reddit',
      sourceId: p.name,
      fetchedAt,
      url: `${BASE}${p.permalink}`,
      title: p.title,
      text: p.selftext || p.title,
      raw: p,
      meta: {
        subreddit: p.subreddit,
        author: p.author,
        score: p.score,
        upvote_ratio: p.upvote_ratio,
        num_comments: p.num_comments,
        created_utc: p.created_utc,
      },
    }));
  }

  private async fetchComments(
    fetcher: typeof globalThis.fetch,
    subreddit: string,
    postId: string,
    limit: number,
    fetchedAt: string,
  ): Promise<RawRecord[]> {
    const url = `${BASE}/r/${subreddit}/comments/${postId}.json?limit=${limit}&raw_json=1`;
    const res = await this.get(fetcher, url) as Array<{ data: { children: CommentChild[] } }>;
    // Reddit returns [postListing, commentsListing]
    const comments = res[1]?.data?.children ?? [];
    return this.flattenComments(comments, fetchedAt);
  }

  private flattenComments(
    items: CommentChild[],
    fetchedAt: string,
  ): RawRecord[] {
    const results: RawRecord[] = [];
    for (const item of items) {
      if (item.kind !== 't1') continue;
      const c = item.data;
      results.push({
        source: 'reddit',
        sourceId: c.name,
        fetchedAt,
        url: `${BASE}${c.permalink}`,
        text: c.body,
        raw: c,
        meta: {
          subreddit: c.subreddit,
          author: c.author,
          score: c.score,
          created_utc: c.created_utc,
          parent_id: c.parent_id,
          link_id: c.link_id,
        },
      });
      if (c.replies?.data?.children) {
        results.push(...this.flattenComments(c.replies.data.children, fetchedAt));
      }
    }
    return results;
  }

  private async get(fetcher: typeof globalThis.fetch, url: string): Promise<unknown> {
    const res = await fetcher(url, { headers: { 'User-Agent': USER_AGENT } });
    if (!res.ok) {
      const body = await res.text();
      throw new Error(`RedditSource: API error ${res.status} ${url}: ${body}`);
    }
    return res.json() as Promise<unknown>;
  }
}
