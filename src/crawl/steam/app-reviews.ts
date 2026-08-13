// appreviews API (store.steampowered.com/appreviews/{appid}?json=1) の取得。
// cursor ページングでレビューを集め、 集計だけ欲しい場合は num_per_page=0 で
// query_summary を引く。 API キー不要。

import { buildSteamHttp } from './http.js';
import type {
  SteamAppReviewRaw,
  SteamAppReviewsResponse,
  SteamHttpOptions,
  SteamReviewSummary,
} from './types.js';

export type FetchAppReviewsOptions = SteamHttpOptions & {
  appId: number;
  /** Steam language フィルタ。 既定 ["all"]。 */
  languages?: string[];
  /** "recent" (cursor 全件走査向き) | "all" | "updated"。 既定 "recent"。 */
  filter?: 'recent' | 'all' | 'updated';
  /** 取得上限。 0/undefined で無制限 (全件)。 */
  maxReviews?: number;
  /** 既知 recommendationid に当たったら打ち切る (増分取得用)。 */
  stopAtRecommendationId?: string;
  /** 1 ページ取得ごとの進捗コールバック (累計件数)。 */
  onPage?: (total: number) => void;
};

/** @implements SPEC-CRAWL-STEAM */
function assertAppId(appId: number): void {
  if (!Number.isSafeInteger(appId) || appId <= 0) {
    throw new Error('steam appId must be a positive integer');
  }
}

/** @implements SPEC-CRAWL-STEAM */
function assertMaxReviews(maxReviews: number | undefined): void {
  if (maxReviews !== undefined && (!Number.isSafeInteger(maxReviews) || maxReviews < 0)) {
    throw new Error('steam maxReviews must be a non-negative integer');
  }
}

/** @implements SPEC-CRAWL-STEAM */
function assertFilter(filter: FetchAppReviewsOptions['filter']): void {
  if (filter !== undefined && filter !== 'recent' && filter !== 'all' && filter !== 'updated') {
    throw new Error('steam filter must be recent, all, or updated');
  }
}

/** @implements SPEC-CRAWL-STEAM */
function buildReviewsUrl(opts: FetchAppReviewsOptions, cursor: string, perPage: number): string {
  const languages = opts.languages && opts.languages.length > 0 ? opts.languages : ['all'];
  return (
    `https://store.steampowered.com/appreviews/${opts.appId}?json=1` +
    `&num_per_page=${perPage}&filter=${opts.filter ?? 'recent'}` +
    `&language=${encodeURIComponent(languages.join(','))}` +
    `&review_type=all&purchase_type=all&cursor=${encodeURIComponent(cursor).replace(/\*/g, '%2A')}`
  );
}

/** @implements SPEC-CRAWL-STEAM */
function assertSuccess(data: SteamAppReviewsResponse, appId: number): void {
  if (data.success !== 1) throw new Error(`steam appreviews success=${data.success} appId=${appId}`);
}

/** @implements SPEC-CRAWL-STEAM — レビュー集計を 1 リクエストで取得する。 */
export async function fetchAppReviewSummary(
  opts: SteamHttpOptions & { appId: number }
): Promise<SteamReviewSummary> {
  assertAppId(opts.appId);
  const http = buildSteamHttp(opts);
  const url = buildReviewsUrl({ appId: opts.appId }, '*', 0);
  const data = (await http.getJson(url)) as SteamAppReviewsResponse;
  assertSuccess(data, opts.appId);
  return data.query_summary ?? {};
}

/** @implements SPEC-CRAWL-STEAM — cursor でレビューを取得する。 */
export async function fetchAppReviews(opts: FetchAppReviewsOptions): Promise<SteamAppReviewRaw[]> {
  assertAppId(opts.appId);
  assertMaxReviews(opts.maxReviews);
  assertFilter(opts.filter);
  const http = buildSteamHttp(opts);
  const out: SteamAppReviewRaw[] = [];
  const seenCursors = new Set<string>();
  let cursor = '*';

  for (;;) {
    const data = (await http.getJson(buildReviewsUrl(opts, cursor, 100))) as SteamAppReviewsResponse;
    assertSuccess(data, opts.appId);

    const reviews = data.reviews ?? [];
    if (reviews.length === 0) break;

    for (const r of reviews) {
      if (opts.stopAtRecommendationId && r.recommendationid === opts.stopAtRecommendationId) {
        return out;
      }
      out.push(r);
      if (opts.maxReviews && out.length >= opts.maxReviews) return out;
    }
    opts.onPage?.(out.length);

    const next = data.cursor;
    if (!next || seenCursors.has(next)) break; // 末尾 (cursor が進まなくなる) で停止
    seenCursors.add(next);
    cursor = next;
    await http.politeWait();
  }

  return out;
}
