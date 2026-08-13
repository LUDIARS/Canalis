// Steam adapter 群を fake fetch で検証する (実 API へは繋がない)。
// - appreviews: cursor ページング / 増分打ち切り / summary
// - store 検索: results_html の決定論パース
// - user reviews: プロフィール HTML の決定論パース / 非公開プロフィール
// - Source 契約: RawRecord の形を固定する

import { describe, it, expect } from 'vitest';
import { fetchAppReviews, fetchAppReviewSummary } from '../src/crawl/steam/app-reviews.js';
import { parseSearchResultsHtml, fetchNewReleases } from '../src/crawl/steam/new-releases.js';
import { parseUserReviewsHtml, fetchUserReviews } from '../src/crawl/steam/user-reviews.js';
import { buildSteamHttp } from '../src/crawl/steam/http.js';
import {
  SteamAppReviewsSource,
  SteamNewReleasesSource,
  SteamUserReviewsSource,
} from '../src/crawl/steam/source.js';
import type { SteamAppReviewRaw } from '../src/crawl/steam/types.js';

const noSleep = () => Promise.resolve();

/** URL→レスポンス本文のマップで応える fake fetch。 */
function fakeFetch(routes: Array<[RegExp, string | object]>): typeof fetch {
  return (async (input: string | URL | Request) => {
    const url = String(input);
    for (const [pattern, body] of routes) {
      if (pattern.test(url)) {
        const text = typeof body === 'string' ? body : JSON.stringify(body);
        return new Response(text, { status: 200 });
      }
    }
    return new Response('not found', { status: 404 });
  }) as typeof fetch;
}

function review(id: string, steamId: string, text = 'good game'): SteamAppReviewRaw {
  return {
    recommendationid: id,
    author: { steamid: steamId, playtime_forever: 120 },
    review: text,
    language: 'japanese',
    timestamp_created: 1754956800,
    voted_up: true,
    votes_up: 3,
  };
}

describe('fetchAppReviews', () => {
  it('cursor を進めて全ページを集める', async () => {
    const fetchImpl = fakeFetch([
      [/cursor=%2A/, { success: 1, reviews: [review('r1', '1'.repeat(17))], cursor: 'c2' }],
      [/cursor=c2/, { success: 1, reviews: [review('r2', '2'.repeat(17))], cursor: 'c3' }],
      [/cursor=c3/, { success: 1, reviews: [], cursor: 'c3' }],
    ]);
    const got = await fetchAppReviews({ appId: 42, fetchImpl, sleepImpl: noSleep });
    expect(got.map((r) => r.recommendationid)).toEqual(['r1', 'r2']);
  });

  it('stopAtRecommendationId で増分取得を打ち切る', async () => {
    const fetchImpl = fakeFetch([
      [
        /cursor=%2A/,
        { success: 1, reviews: [review('r3', '1'.repeat(17)), review('r2', '2'.repeat(17)), review('r1', '3'.repeat(17))], cursor: 'c2' },
      ],
    ]);
    const got = await fetchAppReviews({
      appId: 42,
      fetchImpl,
      sleepImpl: noSleep,
      stopAtRecommendationId: 'r2',
    });
    expect(got.map((r) => r.recommendationid)).toEqual(['r3']);
  });

  it('success!=1 は即エラー (無言 fallback しない)', async () => {
    const fetchImpl = fakeFetch([[/appreviews/, { success: 2 }]]);
    await expect(fetchAppReviews({ appId: 42, fetchImpl, sleepImpl: noSleep })).rejects.toThrow(
      /success=2/
    );
  });

  it('URL を構成する appId / maxReviews は fail-fast する', async () => {
    const fetchImpl = fakeFetch([]);
    await expect(fetchAppReviews({ appId: -1, fetchImpl })).rejects.toThrow(/appId/);
    await expect(fetchAppReviews({ appId: 42, maxReviews: -1, fetchImpl })).rejects.toThrow(
      /maxReviews/
    );
    await expect(
      fetchAppReviews({ appId: 42, filter: 'recent&language=all' as 'recent', fetchImpl })
    ).rejects.toThrow(/filter/);
  });
});

describe('fetchAppReviewSummary', () => {
  it('num_per_page=0 で query_summary を返す', async () => {
    const fetchImpl = fakeFetch([
      [/num_per_page=0/, { success: 1, query_summary: { total_reviews: 321, total_positive: 300 } }],
    ]);
    const got = await fetchAppReviewSummary({ appId: 42, fetchImpl, sleepImpl: noSleep });
    expect(got.total_reviews).toBe(321);
  });
});

const SEARCH_HTML = `
<a href="https://store.steampowered.com/app/111/GameA/?snr=x" data-ds-appid="111" data-ds-itemkey="App_111">
  <span class="title">Game A &amp; Knights</span>
  <div class="col search_released responsive_secondrow">2026 年 8 月 10 日</div>
</a>
<a href="https://store.steampowered.com/app/222/GameB/?snr=x" data-ds-appid="222" data-ds-itemkey="App_222">
  <span class="title">Game B</span>
  <div class="col search_released responsive_secondrow"></div>
</a>`;

describe('parseSearchResultsHtml', () => {
  it('appid / タイトル / リリース日を抽出する', () => {
    const got = parseSearchResultsHtml(SEARCH_HTML);
    expect(got).toEqual([
      {
        appId: 111,
        title: 'Game A & Knights',
        url: 'https://store.steampowered.com/app/111/',
        releaseText: '2026 年 8 月 10 日',
      },
      { appId: 222, title: 'Game B', url: 'https://store.steampowered.com/app/222/', releaseText: undefined },
    ]);
  });
});

describe('fetchNewReleases', () => {
  it('total_count まで start ページングし重複 appid を除く', async () => {
    const page1 = `<a data-ds-appid="111"><span class="title">A</span></a>`;
    const fetchImpl = fakeFetch([
      [/start=0/, { success: 1, results_html: page1, total_count: 1 }],
    ]);
    const got = await fetchNewReleases({ fetchImpl, sleepImpl: noSleep, maxApps: 10 });
    expect(got.map((e) => e.appId)).toEqual([111]);
  });

  it('maxApps が不正ならリクエスト前に失敗する', async () => {
    await expect(fetchNewReleases({ maxApps: -1, fetchImpl: fakeFetch([]) })).rejects.toThrow(/maxApps/);
  });
});

const STEAM_ID = '76561198000000001';
const USER_HTML = `
<div class="review_box ">
  <div class="leftcol">
    <a href="https://steamcommunity.com/profiles/${STEAM_ID}/recommended/111/"></a>
  </div>
  <div class="title"><a href="https://steamcommunity.com/profiles/${STEAM_ID}/recommended/111/">Recommended</a></div>
  <div class="hours">4.9 hrs on record</div>
  <div class="content">
    Great <b>game</b>!<br>Line two &amp; done.
    <div class="spoiler">hidden bit</div>
  </div>
  <div class="posted">Posted 11 August.</div>
</div>
<div class="review_box ">
  <div class="title"><a href="https://steamcommunity.com/profiles/${STEAM_ID}/recommended/222/">Not Recommended</a></div>
  <div class="hours">0.4 hrs on record</div>
  <div class="content">Refunded.</div>
  <div class="posted">Posted 12 August.</div>
</div>`;

describe('parseUserReviewsHtml', () => {
  it('推薦可否 / 本文 (入れ子 div 込み) / 時間 / 投稿日を抽出する', () => {
    const got = parseUserReviewsHtml(USER_HTML, STEAM_ID);
    expect(got).toHaveLength(2);
    expect(got[0]).toMatchObject({
      steamId: STEAM_ID,
      appId: 111,
      recommended: true,
      hoursText: '4.9 hrs on record',
      postedText: 'Posted 11 August.',
    });
    expect(got[0]?.text).toContain('Great game!');
    expect(got[0]?.text).toContain('Line two & done.');
    expect(got[0]?.text).toContain('hidden bit');
    expect(got[1]).toMatchObject({ appId: 222, recommended: false, text: 'Refunded.' });
  });
});

describe('fetchUserReviews', () => {
  it('レビューが尽きるまでページを送る', async () => {
    const fetchImpl = fakeFetch([
      [/p=1/, USER_HTML],
      [/p=2/, '<html>no more</html>'],
    ]);
    const got = await fetchUserReviews({ steamId: STEAM_ID, fetchImpl, sleepImpl: noSleep });
    expect(got.map((e) => e.appId)).toEqual([111, 222]);
  });

  it('非公開プロフィールは空配列 (エラーにしない)', async () => {
    const fetchImpl = fakeFetch([[/p=1/, '<div class="profile_private_info">private</div>']]);
    const got = await fetchUserReviews({ steamId: STEAM_ID, fetchImpl, sleepImpl: noSleep });
    expect(got).toEqual([]);
  });

  it('SteamID64 でない id は fail-fast', async () => {
    const invalidInput = 'untrusted-input';
    await expect(fetchUserReviews({ steamId: invalidInput, fetchImpl: fakeFetch([]) })).rejects.toThrow(
      /SteamID64/
    );
    await expect(fetchUserReviews({ steamId: invalidInput, fetchImpl: fakeFetch([]) })).rejects.not.toThrow(
      invalidInput
    );
  });

  it('maxPages が不正ならリクエスト前に失敗する', async () => {
    await expect(fetchUserReviews({ steamId: STEAM_ID, maxPages: -1, fetchImpl: fakeFetch([]) })).rejects.toThrow(
      /maxPages/
    );
  });
});

describe('Steam HTTP boundary', () => {
  it('redirect を追跡せず、エラーに個人識別子を含む URL を出さない', async () => {
    let requestInit: RequestInit | undefined;
    const fetchImpl = (async (_input: string | URL | Request, init?: RequestInit) => {
      requestInit = init;
      return new Response('no', { status: 404 });
    }) as typeof fetch;
    const http = buildSteamHttp({ fetchImpl });
    const privateUrl = `https://steamcommunity.com/profiles/${STEAM_ID}/recommended/42/`;
    await expect(http.getText(privateUrl)).rejects.not.toThrow(STEAM_ID);
    expect(requestInit?.redirect).toBe('error');
  });

  it('politeDelayMs が不正なら失敗する', () => {
    expect(() => buildSteamHttp({ politeDelayMs: -1 })).toThrow(/politeDelayMs/);
  });
});

describe('Source 契約 (RawRecord 形)', () => {
  const now = () => '2026-08-13T00:00:00.000Z';

  it('SteamAppReviewsSource: レビュー→RawRecord', async () => {
    const fetchImpl = fakeFetch([
      [/cursor=%2A/, { success: 1, reviews: [review('r1', STEAM_ID, 'text1')], cursor: undefined }],
    ]);
    const got = await new SteamAppReviewsSource({ now }).crawl({
      appId: 42,
      fetchImpl,
      sleepImpl: noSleep,
    });
    expect(got).toEqual([
      {
        source: 'steam-app-reviews',
        sourceId: 'steam-review:r1',
        fetchedAt: '2026-08-13T00:00:00.000Z',
        url: `https://steamcommunity.com/profiles/${STEAM_ID}/recommended/42/`,
        text: 'text1',
        raw: review('r1', STEAM_ID, 'text1'),
        meta: {
          appId: 42,
          steamId: STEAM_ID,
          votedUp: true,
          votesUp: 3,
          language: 'japanese',
          timestampCreated: 1754956800,
          playtimeForever: 120,
          numGamesOwned: undefined,
        },
      },
    ]);
  });

  it('SteamNewReleasesSource: 検索エントリ→RawRecord', async () => {
    const fetchImpl = fakeFetch([
      [/start=0/, { success: 1, results_html: SEARCH_HTML, total_count: 2 }],
    ]);
    const got = await new SteamNewReleasesSource({ now }).crawl({
      fetchImpl,
      sleepImpl: noSleep,
      maxApps: 10,
    });
    expect(got[0]).toMatchObject({
      source: 'steam-new-releases',
      sourceId: 'steam-app:111',
      title: 'Game A & Knights',
      meta: { appId: 111, releaseText: '2026 年 8 月 10 日' },
    });
  });

  it('SteamUserReviewsSource: ユーザレビュー→RawRecord', async () => {
    const fetchImpl = fakeFetch([
      [/p=1/, USER_HTML],
      [/p=2/, ''],
    ]);
    const got = await new SteamUserReviewsSource({ now }).crawl({
      steamId: STEAM_ID,
      fetchImpl,
      sleepImpl: noSleep,
    });
    expect(got.map((r) => r.sourceId)).toEqual([
      `steam-user-review:${STEAM_ID}:111`,
      `steam-user-review:${STEAM_ID}:222`,
    ]);
    expect(got[0]?.meta).toMatchObject({ recommended: true, steamId: STEAM_ID });
  });

  it('appId / steamId 欠落は fail-fast', async () => {
    await expect(
      new SteamAppReviewsSource().crawl({ appId: 0 as number, fetchImpl: fakeFetch([]) })
    ).rejects.toThrow(/appId is required/);
    await expect(
      new SteamUserReviewsSource().crawl({ steamId: '', fetchImpl: fakeFetch([]) })
    ).rejects.toThrow(/steamId is required/);
  });
});
