// golden: src/crawl/steam/{new-releases,user-reviews}.ts の HTML 解析と同じ処理 (正規表現のまま) の出力を fixtures に書き出す。
// C++ 版 (正規表現を使わない走査) が同じ結果を返すことを確かめるために使う。
// 使い方: node native/tests/golden/steam_parse.gen.mjs  (Node の組み込みだけを使う)
// new-releases.ts / user-reviews.ts の解析を変えたら、ここも同じに直して生成し直す。

import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// ── new-releases.ts ──
const ROW_SPLIT = /<a\s+[^>]*data-ds-appid="/g;
function stripSearchTags(html) {
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
function parseSearchResultsHtml(html) {
  const out = [];
  const chunks = html.split(ROW_SPLIT).slice(1);
  for (const chunk of chunks) {
    const appIdMatch = /^(\d+)/.exec(chunk);
    if (!appIdMatch) continue;
    const appId = Number(appIdMatch[1]);
    const titleMatch = /<span class="title">([\s\S]*?)<\/span>/.exec(chunk);
    if (!titleMatch) continue;
    const releasedMatch = /<div class="col search_released[^"]*">([\s\S]*?)<\/div>/.exec(chunk);
    out.push({
      appId,
      title: stripSearchTags(titleMatch[1] ?? ''),
      url: `https://store.steampowered.com/app/${appId}/`,
      releaseText: releasedMatch ? stripSearchTags(releasedMatch[1] ?? '') || undefined : undefined,
    });
  }
  return out;
}

// ── user-reviews.ts ──
const REVIEW_BOX_SPLIT = /<div\b[^>]*\bclass="review_box[\s"]/g;
function stripTags(html) {
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
function between(src, startMarker, endMarker) {
  const start = startMarker.exec(src);
  if (!start) return undefined;
  const rest = src.slice(start.index + start[0].length);
  const end = endMarker.exec(rest);
  return end ? rest.slice(0, end.index) : rest;
}
function parseUserReviewsHtml(html, steamId) {
  const out = [];
  const blocks = html.split(REVIEW_BOX_SPLIT).slice(1);
  for (const block of blocks) {
    const appIdMatch = /\/recommended\/(\d+)/.exec(block);
    if (!appIdMatch) continue;
    const appId = Number(appIdMatch[1]);
    const titleHtml = between(block, /<div class="title">/, /<\/div>/) ?? '';
    const title = stripTags(titleHtml);
    const recommended = /not recommended/i.test(title) ? false : /recommended/i.test(title) ? true : undefined;
    const contentHtml = between(
      block,
      /<div class="content(?:\s[^"]*)?"[^>]*>/,
      /<div class="(?:posted|received_compensation|review_rule_violation|reviewer_hardware_container)[\s"]/
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

const STEAM_ID = '76561198000000001';
const search = [
  {
    name: 'ts-fixture',
    html: `
<a href="https://store.steampowered.com/app/111/GameA/?snr=x" data-ds-appid="111" data-ds-itemkey="App_111">
  <span class="title">Game A &amp; Knights</span>
  <div class="col search_released responsive_secondrow">2026 年 8 月 10 日</div>
</a>
<a href="https://store.steampowered.com/app/222/GameB/?snr=x" data-ds-appid="222" data-ds-itemkey="App_222">
  <span class="title">Game B</span>
  <div class="col search_released responsive_secondrow"></div>
</a>`,
  },
  {
    name: 'edge-cases',
    html: `<a data-ds-appid="1,2"><span class="title"><b>Bundle</b>  Pack</span></a>` +
      `<a\tclass="x" data-ds-appid="9" data-ds-appid="333"><span class="title">Last attr wins</span>` +
      `<div class="col search_released x" id="y">bad</div><div class="col search_released">  2026-01-01 </div></a>` +
      `<a data-ds-appid="abc"><span class="title">No digits</span></a>` +
      `<a data-ds-appid="444"><span class="title">No close` +
      `<adata-ds-appid="555"><span class="title">No space</span></a>` +
      `<a data-ds-appid="666">`,
  },
];
const user = [
  {
    name: 'ts-fixture',
    html: `
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
</div>`,
  },
  {
    name: 'ts-fixture-2026-10',
    html: `
<div data-panel="{&quot;flow-children&quot;:&quot;row&quot;}" class="review_box">
  <div class="review_box_content">
    <div class="leftcol"><a href="https://steamcommunity.com/app/333/"></a></div>
    <div class="rightcol">
      <div class="vote_header">
        <div class="title"><a href="https://steamcommunity.com/profiles/${STEAM_ID}/recommended/333/">Recommended</a></div>
        <div class="hours">12.0 hrs on record</div>
      </div>
      <div class="content ">Still fun.<br>Two lines.</div>
      <div class="reviewer_hardware_container"><div class="reviewer_hardware">Steam Deck</div></div>
      <div class="posted">Posted 3 October.</div>
    </div>
  </div>
</div>`,
  },
  {
    name: 'edge-cases',
    html: `<divx class="review_box ">ignored</divx>` +
      `<div id="a" class="review_box"><a href="/recommended/x/"></a><a href="/recommended/77/"></a>` +
      `<div class="title">Something else</div><div class="hours"></div>` +
      `<div class="contentx">no</div><div class="content extra" data-x="1">Line<BR/>A\t\tB<br >\n\n\n\nC &lt;d&gt;` +
      `<div class="received_compensation">paid</div></div>` +
      `<div class="review_box_content"><a href="/recommended/88/"></a></div>` +
      `<div class="review_box"><a href="/recommended/99/"></a><div class="title">Not Recommended</div></div>` +
      `<div class="review_box"><a href="/recommended/100/"></a><div class="content">no end marker <b>bold</b>`,
  },
];

const out = {
  generator: 'native/tests/golden/steam_parse.gen.mjs',
  steamId: STEAM_ID,
  search: search.map((c) => ({ ...c, expected: parseSearchResultsHtml(c.html) })),
  user: user.map((c) => ({ ...c, expected: parseUserReviewsHtml(c.html, STEAM_ID) })),
};
const here = dirname(fileURLToPath(import.meta.url));
writeFileSync(join(here, '..', 'fixtures', 'crawl', 'steam_parse_golden.json'), `${JSON.stringify(out, null, 2)}\n`);
