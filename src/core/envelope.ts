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

/** ②Transform が出力しうる sink 向けエンベロープ。 */
export type SinkBatch = RdbBatch | KgBatch;

/** エンベロープ種別 → sink が受け付ける accepts 値。 raw は RawRecord[] を直接受ける別系統。 */
export type SinkKind = SinkBatch['kind'] | 'raw';
