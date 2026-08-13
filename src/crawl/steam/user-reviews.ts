// steamcommunity プロフィールの recommended ページ (公開プロフィールのみ) から
// 特定ユーザの全レビューを取得する。 per-user レビューには公式 API が無いため、
// HTML を決定論 (マーカー切り出し + タグ除去) で抽出する。

import { buildSteamHttp } from './http.js';
import type { SteamHttpOptions, SteamUserReviewEntry } from './types.js';

export type FetchUserReviewsOptions = SteamHttpOptions & {
  /** SteamID64。 */
  steamId: string;
  /** ページ上限 (1 ページ ~10 件)。 既定 50。 */
  maxPages?: number;
  /** 表示言語。 既定 "english" (判定語 Recommended/Not Recommended の安定のため)。 */
  language?: string;
};

const REVIEW_BOX_SPLIT = /<div class="review_box[\s"]/g;

/** @implements SPEC-CRAWL-STEAM */
function stripTags(html: string): string {
  return html
    .replace(/<br\s*\/?>/gi, '\n')
    .replace(/<[^>]+>/g, '')
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/[ \t]+/g, ' ')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

/** @implements SPEC-CRAWL-STEAM — マーカー間の部分文字列を返す。 */
function between(src: string, startMarker: RegExp, endMarker: RegExp): string | undefined {
  const start = startMarker.exec(src);
  if (!start) return undefined;
  const rest = src.slice(start.index + start[0].length);
  const end = endMarker.exec(rest);
  return end ? rest.slice(0, end.index) : rest;
}

/** @implements SPEC-CRAWL-STEAM — recommended ページの HTML からレビューを抽出する。 */
export function parseUserReviewsHtml(html: string, steamId: string): SteamUserReviewEntry[] {
  const out: SteamUserReviewEntry[] = [];
  const blocks = html.split(REVIEW_BOX_SPLIT).slice(1);
  for (const block of blocks) {
    const appIdMatch = /\/recommended\/(\d+)/.exec(block);
    if (!appIdMatch) continue;
    const appId = Number(appIdMatch[1]);

    const titleHtml = between(block, /<div class="title">/, /<\/div>/) ?? '';
    const title = stripTags(titleHtml);
    const recommended = /not recommended/i.test(title)
      ? false
      : /recommended/i.test(title)
        ? true
        : undefined;

    // content 内には spoiler 等で入れ子 div が入り得るため、 posted (または vote 行) を
    // 終端アンカーにして広めに切り出し、 タグ除去で吸収する。
    const contentHtml = between(
      block,
      /<div class="content"[^>]*>/,
      /<div class="(?:posted|received_compensation|review_rule_violation)"/
    );
    if (contentHtml === undefined) continue;

    const hoursHtml = between(block, /<div class="hours"[^>]*>/, /<\/div>/);
    const postedHtml = between(block, /<div class="posted"[^>]*>/, /<\/div>/);

    out.push({
      steamId,
      appId,
      recommended,
      text: stripTags(contentHtml),
      hoursText: hoursHtml ? stripTags(hoursHtml) || undefined : undefined,
      postedText: postedHtml ? stripTags(postedHtml) || undefined : undefined,
      url: `https://steamcommunity.com/profiles/${steamId}/recommended/${appId}/`,
    });
  }
  return out;
}

/** プロフィールが非公開/存在しない場合に出るマーカー。 */
const PRIVATE_PROFILE_MARKER = /profile_private_info|This profile is private/i;

/** @implements SPEC-CRAWL-STEAM — 指定ユーザのレビューをページ送りで取得する。 */
export async function fetchUserReviews(
  opts: FetchUserReviewsOptions
): Promise<SteamUserReviewEntry[]> {
  if (!/^\d{17}$/.test(opts.steamId)) {
    throw new Error('fetchUserReviews: steamId must be SteamID64 (17 digits)');
  }
  const maxPages = opts.maxPages ?? 50;
  if (!Number.isSafeInteger(maxPages) || maxPages < 0) {
    throw new Error('steam maxPages must be a non-negative integer');
  }
  const http = buildSteamHttp(opts);
  const lang = opts.language ?? 'english';

  const out: SteamUserReviewEntry[] = [];
  const seenApps = new Set<number>();

  for (let page = 1; page <= maxPages; page++) {
    const url = `https://steamcommunity.com/profiles/${opts.steamId}/recommended/?p=${page}&l=${encodeURIComponent(lang)}`;
    const html = await http.getText(url);
    if (page === 1 && PRIVATE_PROFILE_MARKER.test(html)) return [];

    const entries = parseUserReviewsHtml(html, opts.steamId);
    if (entries.length === 0) break;

    let added = 0;
    for (const e of entries) {
      if (seenApps.has(e.appId)) continue; // 末尾ページが繰り返される場合の重複除去
      seenApps.add(e.appId);
      out.push(e);
      added++;
    }
    if (added === 0) break; // 全件既出 = ページが進んでいない
    await http.politeWait();
  }

  return out;
}
