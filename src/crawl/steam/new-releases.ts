// store 検索 (リリース日降順) から新作アプリ一覧を取得する。
// `search/results/?infinite=1` は { results_html, total_count } の JSON を返すので、
// results_html から appid / タイトル / リリース日を決定論 (正規表現) で抽出する。

import { buildSteamHttp } from './http.js';
import type { SteamHttpOptions, SteamSearchEntry } from './types.js';

export type FetchNewReleasesOptions = SteamHttpOptions & {
  /** 取得件数の上限。 既定 100。 */
  maxApps?: number;
  /** store の国コード (価格/リリース日表記に影響)。 既定 "jp"。 */
  countryCode?: string;
  /** 表示言語。 既定 "japanese"。 */
  language?: string;
};

type SearchResultsResponse = {
  success?: number;
  results_html?: string;
  total_count?: number;
};

const PAGE_SIZE = 50;

/** 検索結果 1 行分の anchor ブロック。 data-ds-appid を持つ <a> 単位で切る。 */
const ROW_SPLIT = /<a\s+[^>]*data-ds-appid="/g;

/** @implements SPEC-CRAWL-STEAM */
function stripTags(html: string): string {
  return html
    .replace(/<[^>]+>/g, ' ')
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/\s+/g, ' ')
    .trim();
}

/** @implements SPEC-CRAWL-STEAM — results_html から検索エントリを抽出する。 */
export function parseSearchResultsHtml(html: string): SteamSearchEntry[] {
  const out: SteamSearchEntry[] = [];
  const chunks = html.split(ROW_SPLIT).slice(1); // 先頭はマッチ前の前置き
  for (const chunk of chunks) {
    const appIdMatch = /^(\d+)/.exec(chunk);
    if (!appIdMatch) continue;
    const appId = Number(appIdMatch[1]);
    // バンドル等で appid がカンマ区切りになる行 (data-ds-appid="1,2") は先頭 id を採る。
    const titleMatch = /<span class="title">([\s\S]*?)<\/span>/.exec(chunk);
    if (!titleMatch) continue;
    const releasedMatch = /<div class="col search_released[^"]*">([\s\S]*?)<\/div>/.exec(chunk);
    out.push({
      appId,
      title: stripTags(titleMatch[1] ?? ''),
      url: `https://store.steampowered.com/app/${appId}/`,
      releaseText: releasedMatch ? stripTags(releasedMatch[1] ?? '') || undefined : undefined,
    });
  }
  return out;
}

/** @implements SPEC-CRAWL-STEAM — リリース日降順の検索をページング取得する。 */
export async function fetchNewReleases(
  opts: FetchNewReleasesOptions = {}
): Promise<SteamSearchEntry[]> {
  const maxApps = opts.maxApps ?? 100;
  if (!Number.isSafeInteger(maxApps) || maxApps < 0) {
    throw new Error('steam maxApps must be a non-negative integer');
  }
  const http = buildSteamHttp(opts);
  const cc = opts.countryCode ?? 'jp';
  const lang = opts.language ?? 'japanese';

  const out: SteamSearchEntry[] = [];
  const seen = new Set<number>();

  for (let start = 0; out.length < maxApps; start += PAGE_SIZE) {
    const url =
      `https://store.steampowered.com/search/results/?query&start=${start}&count=${PAGE_SIZE}` +
      `&sort_by=Released_DESC&category1=998&infinite=1&cc=${encodeURIComponent(cc)}` +
      `&l=${encodeURIComponent(lang)}`;
    const data = (await http.getJson(url)) as SearchResultsResponse;
    if (data.success !== undefined && data.success !== 1) {
      throw new Error(`steam search success=${data.success}`);
    }
    const entries = parseSearchResultsHtml(data.results_html ?? '');
    if (entries.length === 0) break;

    for (const e of entries) {
      if (seen.has(e.appId)) continue; // ページ跨ぎの重複 (掲載順変動) を除去
      seen.add(e.appId);
      out.push(e);
      if (out.length >= maxApps) break;
    }
    if (data.total_count !== undefined && start + PAGE_SIZE >= data.total_count) break;
    await http.politeWait();
  }

  return out;
}
