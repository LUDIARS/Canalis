// Steam adapter の型定義。 store.steampowered.com / steamcommunity.com の
// 公開エンドポイント (API キー不要) が返す形をそのまま写した raw 型と、
// adapter 共通のオプションを持つ。

/** appreviews API のレビュー投稿者 (公開属性)。 */
export type SteamReviewAuthor = {
  /** SteamID64。 公開・安定な同一性アンカー。 */
  steamid?: string;
  num_games_owned?: number;
  num_reviews?: number;
  playtime_forever?: number;
  playtime_at_review?: number;
};

/** appreviews API のレビュー 1 件 (verbatim)。 */
export type SteamAppReviewRaw = {
  recommendationid: string;
  author?: SteamReviewAuthor;
  review: string;
  language?: string;
  /** epoch 秒。 */
  timestamp_created: number;
  timestamp_updated?: number;
  voted_up: boolean;
  votes_up?: number;
  votes_funny?: number;
  steam_purchase?: boolean;
  received_for_free?: boolean;
  written_during_early_access?: boolean;
};

/** appreviews API の query_summary (num_per_page=0 でも返る集計)。 */
export type SteamReviewSummary = {
  review_score?: number;
  review_score_desc?: string;
  total_positive?: number;
  total_negative?: number;
  total_reviews?: number;
};

/** appreviews API レスポンス。 */
export type SteamAppReviewsResponse = {
  success: number;
  reviews?: SteamAppReviewRaw[];
  cursor?: string;
  query_summary?: SteamReviewSummary;
};

/** store 検索 (新作一覧) から抽出した 1 アプリ。 */
export type SteamSearchEntry = {
  appId: number;
  title: string;
  url: string;
  /** 検索結果に表示されるリリース日テキスト (ロケール依存の生文字列)。 */
  releaseText?: string;
};

/** steamcommunity プロフィールの recommended ページから抽出した 1 レビュー。 */
export type SteamUserReviewEntry = {
  steamId: string;
  appId: number;
  /** おすすめ (true) / おすすめしない (false)。 判定不能は undefined。 */
  recommended?: boolean;
  /** レビュー本文 (タグ除去済みプレーンテキスト)。 */
  text: string;
  /** "4.9 hrs on record" 等の生文字列。 */
  hoursText?: string;
  /** "Posted 11 August." 等の生文字列。 */
  postedText?: string;
  url: string;
};

/** 全 Steam adapter 共通の HTTP オプション。 */
export type SteamHttpOptions = {
  /** DI 用。 既定 global fetch。 */
  fetchImpl?: typeof fetch;
  /** 同一ドメインへの最小リクエスト間隔 ms。 既定 1200。 */
  politeDelayMs?: number;
  /** テスト決定性のための待機実装。 既定 setTimeout。 */
  sleepImpl?: (ms: number) => Promise<void>;
};
