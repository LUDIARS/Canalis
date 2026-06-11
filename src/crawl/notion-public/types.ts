// notion-public クローラーの型定義。
// API トークン不要の公開 Notion ページを Playwright でレンダリング取得する。

/** Source<NotionPublicSourceConfig> に渡す設定。 */
export type NotionPublicSourceConfig = {
  /** クロール対象の公開 Notion ページ URL。 */
  url: string;
  options?: NotionPublicCrawlOptions;
};

export type NotionPublicCrawlOptions = {
  /** ページ全体のタイムアウト (ms)。 デフォルト 30000。 */
  timeout?: number;
  /** スクロール間の待機時間 (ms)。 デフォルト 800。 */
  scrollDelay?: number;
  /** 最大スクロール回数。 デフォルト 15。 */
  maxScrolls?: number;
};

/** PlaywrightFetcher に渡す解決済みオプション。 */
export type FetchPageOptions = {
  timeout: number;
  scrollDelay: number;
  maxScrolls: number;
};

/** ブラウザから抽出した 1 ブロック。 */
export type ExtractedBlock = {
  /** Markdown 変換用のブロック種別。 h1 / h2 / h3 / li / oli / quote / code / callout / divider / text */
  type: string;
  text: string;
};

/** ブラウザ内で抽出したページ全体のデータ。 */
export type ExtractionResult = {
  title: string;
  blocks: ExtractedBlock[];
};

/** ページ取得の抽象 (テスト時に差し替え可能)。 */
export interface PageFetcher {
  fetch(url: string, options: FetchPageOptions): Promise<FetchedPage>;
}

/** PageFetcher の戻り値。 */
export type FetchedPage = {
  url: string;
  title: string;
  /** blocksToMarkdown で生成した本文 Markdown。 */
  markdown: string;
  /** verbatim 保持用の生データ。 */
  raw: ExtractionResult;
};
