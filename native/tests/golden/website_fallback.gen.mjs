// golden: src/crawl/website/source.ts の fallbackParser / toSourceId と同じ処理 (正規表現のまま) の出力を fixtures に書き出す。
// C++ 版 (正規表現を使わない線形の走査) が同じ結果を返すことを確かめるために使う。
// 使い方: node native/tests/golden/website_fallback.gen.mjs  (Node の組み込みだけを使う)
// source.ts の fallbackParser / decodeHtmlEntities / toSourceId を変えたら、ここも同じに直して生成し直す。

import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

function decodeHtmlEntities(s) {
  return s
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&nbsp;/g, ' ');
}

function parse(html, url) {
  const titleMatch = /<title[^>]*>([\s\S]*?)<\/title>/i.exec(html);
  const title = titleMatch?.[1] != null ? decodeHtmlEntities(titleMatch[1].trim()) : undefined;
  const stripped = html
    .replace(/<script[\s\S]*?<\/script>/gi, ' ')
    .replace(/<style[\s\S]*?<\/style>/gi, ' ')
    .replace(/<[^>]+>/g, ' ')
    .replace(/\s{2,}/g, ' ')
    .trim();
  const text = decodeHtmlEntities(stripped).slice(0, 50_000);
  const base = (() => { try { return new URL(url).origin; } catch { return ''; } })();
  const links = [];
  for (const m of html.matchAll(/href="([^"]+)"/gi)) {
    const href = m[1];
    if (!href) continue;
    try {
      const abs = new URL(href, url).href;
      if (base && abs.startsWith(base)) links.push(abs);
    } catch { /* skip invalid URLs */ }
  }
  return { title, text, links: [...new Set(links)].slice(0, 100) };
}

function toSourceId(url) {
  try {
    const u = new URL(url);
    return `${u.hostname}${u.pathname}`.replace(/[^a-zA-Z0-9._/-]/g, '_').slice(0, 200);
  } catch {
    return url.slice(0, 200);
  }
}

const cases = [
  {
    name: 'stub',
    url: 'https://example.com/',
    html: `<!DOCTYPE html>\n<html>\n<head><title>Test Page &amp; More</title></head>\n<body>\n<p>Hello <strong>world</strong>!</p>\n<a href="/about">About</a>\n<a href="https://external.com/page">External</a>\n<script>alert('skip me')</script>\n</body>\n</html>`,
  },
  {
    name: 'mixed-case-tags-and-entities',
    url: 'https://example.com/docs/guide/index.html',
    html: `<HTML><HEAD><TITLE lang="ja">  ガイド &lt;入門&gt; &amp;amp;  </TITLE><STYLE>p{color:red}</STYLE></HEAD>` +
      `<BODY>\t<P>一行目&nbsp;&nbsp;です</P>\n\n\n<p>a &quot;b&quot; &#39;c&#39;</p>` +
      `<A HREF="../intro">intro</A><a href="sub/page?x=1#h">sub</a><a href="#top">top</a><a href="/about">dup</a>` +
      `<a href="/about">dup2</a><a href="mailto:x@example.com">mail</a><a data-href="/data">data</a><a href="">empty</a>` +
      `<SCRIPT type="text/javascript">var s = "<p>no</p>";</SCRIPT>終わり 1 < 2 > 0 <> end</BODY></HTML>`,
  },
  {
    name: 'no-title-unclosed-script',
    url: 'https://example.com:8443/a b/日本',
    html: `<p>before</p><script>never closed <p>still text</p>`,
  },
  {
    name: 'unicode-whitespace',
    url: 'https://example.com/',
    html: `<title>x</title><p>a　　b c\r\n\r\nd</p>`,
  },
];

const out = {
  generator: 'native/tests/golden/website_fallback.gen.mjs',
  cases: cases.map((c) => ({ ...c, expected: { ...parse(c.html, c.url), sourceId: toSourceId(c.url) } })),
};
const here = dirname(fileURLToPath(import.meta.url));
writeFileSync(join(here, '..', 'fixtures', 'crawl', 'website_fallback_golden.json'), `${JSON.stringify(out, null, 2)}\n`);
