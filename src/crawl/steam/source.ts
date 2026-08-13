// ①Crawl adapter: Steam の 3 取得系 (新作一覧 / アプリ別レビュー / ユーザ別レビュー) を
// それぞれ Source 契約に適合させる薄いラッパ。 取得本体は各モジュールの関数。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import { fetchAppReviews, type FetchAppReviewsOptions } from './app-reviews.js';
import { fetchNewReleases, type FetchNewReleasesOptions } from './new-releases.js';
import { fetchUserReviews, type FetchUserReviewsOptions } from './user-reviews.js';

export type SteamSourceDeps = {
  /** fetchedAt の時刻源 (テスト決定性のため注入可能)。 */
  now?: () => string;
};

const defaultNow = () => new Date().toISOString();

/** @implements SPEC-CRAWL-STEAM — 新作アプリ一覧を RawRecord[] にする adapter。 */
export class SteamNewReleasesSource implements Source<FetchNewReleasesOptions> {
  readonly name = 'steam-new-releases';

  constructor(private readonly deps: SteamSourceDeps = {}) {}

  async crawl(config: FetchNewReleasesOptions = {}): Promise<RawRecord[]> {
    const entries = await fetchNewReleases(config);
    const fetchedAt = (this.deps.now ?? defaultNow)();
    return entries.map((e) => ({
      source: 'steam-new-releases',
      sourceId: `steam-app:${e.appId}`,
      fetchedAt,
      url: e.url,
      title: e.title,
      raw: e,
      meta: { appId: e.appId, releaseText: e.releaseText },
    }));
  }
}

/** @implements SPEC-CRAWL-STEAM — アプリのレビュー群を RawRecord[] にする adapter。 */
export class SteamAppReviewsSource implements Source<FetchAppReviewsOptions> {
  readonly name = 'steam-app-reviews';

  constructor(private readonly deps: SteamSourceDeps = {}) {}

  async crawl(config: FetchAppReviewsOptions): Promise<RawRecord[]> {
    if (!config?.appId) throw new Error('SteamAppReviewsSource: appId is required');
    const reviews = await fetchAppReviews(config);
    const fetchedAt = (this.deps.now ?? defaultNow)();
    return reviews.map((r) => {
      const steamId = r.author?.steamid;
      return {
        source: 'steam-app-reviews',
        sourceId: `steam-review:${r.recommendationid}`,
        fetchedAt,
        url: steamId
          ? `https://steamcommunity.com/profiles/${steamId}/recommended/${config.appId}/`
          : undefined,
        text: r.review || undefined,
        raw: r,
        meta: {
          appId: config.appId,
          // SteamID64 = 公開・安定な同一性アンカー (同一人物の横断同定に使う)
          steamId,
          votedUp: r.voted_up,
          votesUp: r.votes_up,
          language: r.language,
          timestampCreated: r.timestamp_created,
          playtimeForever: r.author?.playtime_forever,
          numGamesOwned: r.author?.num_games_owned,
        },
      };
    });
  }
}

/** @implements SPEC-CRAWL-STEAM — ユーザの全レビューを RawRecord[] にする adapter。 */
export class SteamUserReviewsSource implements Source<FetchUserReviewsOptions> {
  readonly name = 'steam-user-reviews';

  constructor(private readonly deps: SteamSourceDeps = {}) {}

  async crawl(config: FetchUserReviewsOptions): Promise<RawRecord[]> {
    if (!config?.steamId) throw new Error('SteamUserReviewsSource: steamId is required');
    const entries = await fetchUserReviews(config);
    const fetchedAt = (this.deps.now ?? defaultNow)();
    return entries.map((e) => ({
      source: 'steam-user-reviews',
      sourceId: `steam-user-review:${e.steamId}:${e.appId}`,
      fetchedAt,
      url: e.url,
      text: e.text || undefined,
      raw: e,
      meta: {
        appId: e.appId,
        steamId: e.steamId,
        recommended: e.recommended,
        hoursText: e.hoursText,
        postedText: e.postedText,
      },
    }));
  }
}
