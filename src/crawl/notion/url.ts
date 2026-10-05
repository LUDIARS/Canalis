// Notion の URL ⇄ page id の決定論変換。 本文中の Notion リンク検出も持つ。
// 対応: www.notion.so / notion.so / *.notion.site。 `?p=<id>` (peek 表示) は path より優先。

const NOTION_HOST_RE = /(^|\.)notion\.(so|site)$/i;
const ID32_RE = /([0-9a-f]{32})$/i;
const UUID_RE = /([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$/i;
const URL_IN_TEXT_RE = /https?:\/\/[^\s<>()"'`]+/gi;

function toUrl(input: string): URL | null {
  try {
    return new URL(input.trim());
  } catch {
    return null;
  }
}

/** 32 桁 hex を dash 付き UUID 形式に整える。 */
function formatUuid(hex: string): string {
  const h = hex.toLowerCase();
  return `${h.slice(0, 8)}-${h.slice(8, 12)}-${h.slice(12, 16)}-${h.slice(16, 20)}-${h.slice(20)}`;
}

/** 末尾の id (32hex または UUID) を取り出す。 */
function idFromSegment(segment: string): string | null {
  const uuid = UUID_RE.exec(segment);
  if (uuid) return uuid[1]!.toLowerCase();
  const hex = ID32_RE.exec(segment);
  return hex ? formatUuid(hex[1]!) : null;
}

/** Notion のホスト (notion.so / *.notion.site) を持つ URL か。 */
export function isNotionUrl(input: string): boolean {
  const u = toUrl(input);
  return !!u && /^https?:$/.test(u.protocol) && NOTION_HOST_RE.test(u.hostname);
}

/**
 * Notion URL から page (または database) id を UUID 形式で取り出す。
 * Notion URL でない / id を含まないなら null。
 */
export function parseNotionPageId(input: string): string | null {
  if (!isNotionUrl(input)) return null;
  const u = toUrl(input)!;
  const peek = u.searchParams.get('p');
  if (peek) {
    const id = idFromSegment(peek);
    if (id) return id;
  }
  const segments = u.pathname.split('/').filter(Boolean);
  const last = segments[segments.length - 1];
  return last ? idFromSegment(decodeURIComponent(last)) : null;
}

/** 本文中の Notion URL を出現順・重複なしで列挙する (末尾の句読点は除く)。 */
export function findNotionUrls(text: string): string[] {
  const out: string[] = [];
  for (const m of text.matchAll(URL_IN_TEXT_RE)) {
    const url = m[0].replace(/[.,;:!?、。」』）\]]+$/u, '');
    if (isNotionUrl(url) && !out.includes(url)) out.push(url);
  }
  return out;
}
