// ステージ ②Clean → ③Save の受け渡し契約 = sink ネイティブの「最終形」エンベロープ。
//
// 設計の肝: 整形の知識はすべて ②(サービス固有) が持ち、 ②が保存先のネイティブ形まで作り切る。
// ③Save は渡されたエンベロープを verbatim で書くだけ (table-map も射影も持たない)。
// よって ③ writer は sink エンジン 1 種につき 1 個で全サービスを使い回せる。

/** RDB の 1 行。 */
export type Row = Record<string, unknown>;

/** RDB への upsert 1 単位。 conflictKey での衝突は値マージ更新を意図する。 */
export type RdbUpsert = {
  table: string;
  rows: Row[];
  /** ON CONFLICT 対象カラム。 */
  conflictKey: string[];
};

/** RDB sink (Postgres 等) 用エンベロープ。 */
export type RdbBatch = {
  kind: 'rdb';
  upserts: RdbUpsert[];
};

/** グラフのノード参照 (エッジの端点)。 */
export type NodeRef = { label: string; key: string };

/** グラフのノード。 key は label 内で一意 (MERGE キー)。 */
export type GraphNode = {
  label: string;
  key: string;
  props?: Record<string, unknown>;
};

/** グラフのエッジ (有向)。 */
export type GraphEdge = {
  label: string;
  from: NodeRef;
  to: NodeRef;
  props?: Record<string, unknown>;
};

/** KG sink (Kuzu 等) 用エンベロープ。 */
export type KgBatch = {
  kind: 'kg';
  nodes: GraphNode[];
  edges: GraphEdge[];
};

// --- FT (学習的インポート) 用エンベロープ -------------------------------------
// ②が「学習例」まで作り、 ③(FtSink)はデータセット化 + 外部 runner 起動だけを行う。
// Canalis core は学習を持たない (LLM/ML 依存ゼロ)。 実際の FT は core 外の runner (Python 等)。

/** FT のタスク種別。 A=分類器, B=生成 LLM の両対応。 */
export type FtTask = 'classification' | 'causal-lm';

/** 分類タスクの 1 例 (text → label)。 */
export type FtClassificationExample = { input: string; label: string };

/** 生成タスクの 1 例。 chat 形式 または prompt/completion 形式。 */
export type FtChatExample = { messages: { role: string; content: string }[] };
export type FtPromptExample = { prompt: string; completion: string };

/** FT の 1 学習例 (task により形が変わる。 runner が task を見て解釈)。 */
export type FtExample = FtClassificationExample | FtChatExample | FtPromptExample;

/** FT sink 用エンベロープ。 runner へ渡す「学習データ + ジョブ設定」。 */
export type FtBatch = {
  kind: 'ft';
  task: FtTask;
  examples: FtExample[];
  /** データセット名 (出力先サブフォルダ / バージョン管理に使う)。 */
  dataset?: string;
  /** base model / adapter / ハイパラ等、 runner が解釈する設定 (core は中身を見ない)。 */
  model?: Record<string, unknown>;
};

/** ②Transform が出力しうる sink 向けエンベロープ。 */
export type SinkBatch = RdbBatch | KgBatch | FtBatch;

/** エンベロープ種別 → sink が受け付ける accepts 値。 raw は RawRecord[] を直接受ける別系統。 */
export type SinkKind = SinkBatch['kind'] | 'raw';
