// ①Crawl adapter: URL から HTML を取得し RawRecord[] へ変換する。
// HTML → 構造化テキストへの変換は HtmlParser を注入 (Lector 等を接続する口)。
// パーサ未注入時はフォールバック実装でタグ除去 + タイトル抽出を行う。

import type { RawRecord } from '../../core/raw-record.js';
import type { Source } from '../../core/pipeline.js';
import type { WebsiteSourceConfig, HtmlParser } from './types.js';

const DEFAULT_UA = 'Canalis-Crawler/1.0 (LUDIARS; +https://github.com/LUDIARS)';
const DEFAULT_TIMEOUT = 10_000;

export type WebsiteSourceDeps = {
  fetch?: typeof globalThis.fetch;
  now?: () => string;
  /** HTML → 構造化テキストのパーサ (Lector 等)。 省略時はフォールバック実装を使う。 */
  htmlParser?: HtmlParser;
};

/** Web ページを 1 source として扱う ①Crawl adapter。 */
export class WebsiteSource implements Source<WebsiteSourceConfig> {
  readonly name = 'website';

  constructor(private readonly deps: WebsiteSourceDeps = {}) {}

  async crawl(config: WebsiteSourceConfig): Promise<RawRecord[]> {
    if (!config.url) throw new Error('WebsiteSource: url is required');

    const fetcher = this.deps.fetch ?? globalThis.fetch;
    const now = this.deps.now ?? (() => new Date().toISOString());
    const ua = config.userAgent ?? DEFAULT_UA;
    const timeoutMs = config.timeoutMs ?? DEFAULT_TIMEOUT;

    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);

    let html: string;
    let finalUrl = config.url;
    try {
      const res = await fetcher(config.url, {
        headers: { 'User-Agent': ua },
        signal: controller.signal,
      });
      finalUrl = res.url || config.url;
      if (!res.ok) throw new Error(`WebsiteSource: HTTP ${res.status} ${config.url}`);
      html = await res.text();
    } finally {
      clearTimeout(timer);
    }

    const parser = this.deps.htmlParser ?? fallbackParser;
    const parsed = parser.parse(html, finalUrl);
    const fetchedAt = now();
    const sourceId = toSourceId(finalUrl);

    return [
      {
        source: 'website',
        sourceId,
        fetchedAt,
        url: finalUrl,
        title: parsed.title,
        text: parsed.text,
        raw: { html, url: finalUrl },
        meta: { links: parsed.links, originalUrl: config.url },
      },
    ];
  }
}

function toSourceId(url: string): string {
  // URL を安定した ASCII 文字列 ID にする (パスまで含む)
  try {
    const u = new URL(url);
    return `${u.hostname}${u.pathname}`.replace(/[^a-zA-Z0-9._/-]/g, '_').slice(0, 200);
  } catch {
    return url.slice(0, 200);
  }
}

/** タグ除去 + タイトル抽出の最小フォールバック実装。 */
const fallbackParser: HtmlParser = {
  parse(html: string, url: string) {
    const titleMatch = /<title[^>]*>([\s\S]*?)<\/title>/i.exec(html);
    const title = titleMatch?.[1] != null ? decodeHtmlEntities(titleMatch[1].trim()) : undefined;

    // script / style を除去してからタグを剥がす
    const stripped = html
      .replace(/<script[\s\S]*?<\/script>/gi, ' ')
      .replace(/<style[\s\S]*?<\/style>/gi, ' ')
      .replace(/<[^>]+>/g, ' ')
      .replace(/\s{2,}/g, ' ')
      .trim();
    const text = decodeHtmlEntities(stripped).slice(0, 50_000);

    // 同一ドメインのリンクを抽出
    const base = (() => { try { return new URL(url).origin; } catch { return ''; } })();
    const links: string[] = [];
    for (const m of html.matchAll(/href="([^"]+)"/gi)) {
      const href = m[1];
      if (!href) continue;
      try {
        const abs = new URL(href, url).href;
        if (base && abs.startsWith(base)) links.push(abs);
      } catch { /* skip invalid URLs */ }
    }

    return { title, text, links: [...new Set(links)].slice(0, 100) };
  },
};

function decodeHtmlEntities(s: string): string {
  return s
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&nbsp;/g, ' ');
}
