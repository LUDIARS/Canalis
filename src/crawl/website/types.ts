export type WebsiteSourceConfig = {
  url: string;
  /** User-Agent (デフォルト: Canalis-Crawler/1.0)。 */
  userAgent?: string;
  /** タイムアウト ms (デフォルト 10000)。 */
  timeoutMs?: number;
};

/** HTML → 構造化テキストへ変換するパーサの口 (Lector 等を注入)。 */
export interface HtmlParser {
  parse(html: string, url: string): ParsedPage;
}

export type ParsedPage = {
  title?: string;
  /** 本文テキスト (Markdown 等)。 */
  text: string;
  /** 同一ドメイン等のリンク一覧。 */
  links: string[];
};
