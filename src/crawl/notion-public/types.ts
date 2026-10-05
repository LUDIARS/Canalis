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
  /** 本文中の同一サイト Notion ページへのリンク (絶対 URL・重複なし)。 子ページ辿りに使う。 */
  links?: string[];
};

/** ページ取得の抽象 (テスト時に差し替え可能)。 */
export interface PageFetcher {
  fetch(url: string, options: FetchPageOptions): Promise<FetchedPage>;
  /** 保持しているブラウザ等を解放する (複数ページ取得後に 1 回呼ぶ)。 */
  close?(): Promise<void>;
}

/** crawlPublicPages の設定。 */
export type NotionPublicPageCrawlOptions = NotionPublicCrawlOptions & {
  /** 起点ページ (=0) から辿るリンク先ページの深さ。 既定 0 (起点のみ)。 */
  maxDepth?: number;
  /** 取得する最大ページ数 (安全弁)。 既定 50。 */
  maxPages?: number;
};

/** crawlPublicPages で得た 1 ページ。 */
export type NotionPublicCrawledPage = {
  url: string;
  title: string;
  depth: number;
  /** リンク元ページの URL (起点は '')。 */
  parentUrl: string;
  markdown: string;
};

export type NotionPublicCrawlResult = {
  rootUrl: string;
  pages: NotionPublicCrawledPage[];
  errors: { url: string; message: string }[];
  /** maxPages で打ち切ったか */
  truncated: boolean;
};

/** PageFetcher の戻り値。 */
export type FetchedPage = {
  url: string;
  title: string;
  /** blocksToMarkdown で生成した本文 Markdown。 */
  markdown: string;
  /** verbatim 保持用の生データ。 */
  raw: ExtractionResult;
};
