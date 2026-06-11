// ①Crawl adapter: YouTube Data API v3 でコメントスレッドを取得し RawRecord[] へ変換する。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import type { YouTubeSourceConfig, YouTubeCommentThread } from './types.js';

const BASE = 'https://www.googleapis.com/youtube/v3';

export type YouTubeSourceDeps = {
  fetch?: typeof globalThis.fetch;
  now?: () => string;
};

/** YouTube コメントスレッドを 1 source として扱う ①Crawl adapter。 */
export class YouTubeSource implements Source<YouTubeSourceConfig> {
  readonly name = 'youtube';

  constructor(private readonly deps: YouTubeSourceDeps = {}) {}

  async crawl(config: YouTubeSourceConfig): Promise<RawRecord[]> {
    if (!config.videoId) throw new Error('YouTubeSource: videoId is required');
    if (!config.apiKey) throw new Error('YouTubeSource: apiKey is required');

    const fetcher = this.deps.fetch ?? globalThis.fetch;
    const now = this.deps.now ?? (() => new Date().toISOString());
    const maxResults = config.maxResults ?? 100;
    const textFormat = config.textFormat ?? 'plainText';

    const threads = await this.fetchAllComments(fetcher, config.videoId, config.apiKey, maxResults, textFormat);
    const fetchedAt = now();
    const videoUrl = `https://www.youtube.com/watch?v=${config.videoId}`;

    return threads.map((t) => {
      const snippet = t.snippet.topLevelComment.snippet;
      return {
        source: 'youtube',
        sourceId: t.id,
        fetchedAt,
        url: videoUrl,
        title: undefined,
        text: snippet.textDisplay,
        raw: t,
        meta: {
          videoId: config.videoId,
          authorDisplayName: snippet.authorDisplayName,
          authorChannelId: snippet.authorChannelId?.value,
          likeCount: snippet.likeCount,
          replyCount: t.snippet.totalReplyCount,
          publishedAt: snippet.publishedAt,
          updatedAt: snippet.updatedAt,
        },
      };
    });
  }

  private async fetchAllComments(
    fetcher: typeof globalThis.fetch,
    videoId: string,
    apiKey: string,
    maxResults: number,
    textFormat: string,
  ): Promise<YouTubeCommentThread[]> {
    const results: YouTubeCommentThread[] = [];
    let pageToken: string | undefined;

    do {
      const params = new URLSearchParams({
        part: 'snippet',
        videoId,
        key: apiKey,
        maxResults: String(Math.min(maxResults - results.length, 100)),
        textFormat,
      });
      if (pageToken) params.set('pageToken', pageToken);

      const res = await fetcher(`${BASE}/commentThreads?${params}`);
      if (!res.ok) {
        const body = await res.text();
        throw new Error(`YouTubeSource: API error ${res.status}: ${body}`);
      }

      const data = (await res.json()) as {
        items: YouTubeCommentThread[];
        nextPageToken?: string;
      };

      results.push(...data.items);
      pageToken = data.nextPageToken;
    } while (pageToken && results.length < maxResults);

    return results;
  }
}
