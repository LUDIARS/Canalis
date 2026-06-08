// ステージ ①Crawl → ②Clean、 および ①Crawl → raw保存 の受け渡し契約。
// source 非依存の正規化された生レコード。 raw には source ネイティブの原データを verbatim で持つ
// (これにより rawSave した RawRecord[] を後から再処理 = replay できる)。

/** 1 件の取得結果。 ②Transform と raw sink の両方がこの形を受け取る。 */
export type RawRecord = {
  /** 取得元の種別。 例: "notion" | "youtube" | "reddit" | "website" */
  source: string;
  /** source 内で一意な ID (dedup キー)。 例: Notion page id, YouTube comment id */
  sourceId: string;
  /** 取得時刻 (ISO8601 / UTC)。 */
  fetchedAt: string;
  /** 元 URL (あれば)。 */
  url?: string;
  /** タイトル/見出し (あれば)。 */
  title?: string;
  /** 本文を正規化したテキスト (Markdown 等)。 ②の入力主体。 */
  text?: string;
  /** source ネイティブの原データを verbatim で保持 (replay / 監査用)。 */
  raw: unknown;
  /** 付帯メタ (source 固有の補助情報)。 */
  meta?: Record<string, unknown>;
};
