# data: sink-native エンベロープ (②→③ の受け渡しスキーマ)

②Transform が出力し ③Save が verbatim 書込する「保存先ネイティブの最終形」。
整形の知識は②が持ち切り、③は `kind` で振り分けてそのまま書くだけ (table-map / 射影を持たない)。

正本: `src/core/envelope.ts`。

## 種別 (`SinkBatch` / `SinkKind`)

`SinkBatch = RdbBatch | KgBatch | FtBatch`。`SinkKind = 'rdb' | 'kg' | 'ft' | 'raw'`。
`raw` は `RawRecord[]` を直接受ける別系統 (エンベロープを経由しない)。
runner は sink の `accepts` と manifest 宣言の `accepts` 一致を要求する (不一致は throw)。

### RdbBatch (`kind: 'rdb'`) — Postgres 等

```ts
type RdbBatch = { kind: 'rdb'; upserts: RdbUpsert[] };
type RdbUpsert = { table: string; rows: Row[]; conflictKey: string[] };
type Row = Record<string, unknown>;
```

- `PostgresSink` が 1 行 = 1 `INSERT ... ON CONFLICT (conflictKey) DO UPDATE SET …` に展開。
- `conflictKey` 以外の列が無ければ `DO NOTHING`。
- 識別子は `"…"` で引用 (`"` は重ねる)。`[A-Za-z_][A-Za-z0-9_]*` 以外で `"` を含むものは fail-fast。

### KgBatch (`kind: 'kg'`) — Kuzu 等

```ts
type KgBatch = { kind: 'kg'; nodes: GraphNode[]; edges: GraphEdge[] };
type GraphNode = { label: string; key: string; props?: Record<string, unknown> };
type GraphEdge = { label: string; from: NodeRef; to: NodeRef; props?: Record<string, unknown> };
type NodeRef = { label: string; key: string };
```

- `KuzuSink` が node を `MERGE (n:label {key: $key}) SET …`、edge を `MATCH … MERGE (a)-[r:label]->(b) SET …` に展開。
- MERGE キーのプロパティ名は `KuzuSinkConfig.keyProp` (既定 `key`)。
- Kuzu は `SET n += $map` 非対応のため props は `alias.k = $p_k` 形式で個別展開。
- label は `[A-Za-z_][A-Za-z0-9_]*` のみ許可 (それ以外は throw)。
- **前提**: node/edge のテーブル定義 (厳格スキーマ) は事前マイグレーション済であること = サービスの責務。

### FtBatch (`kind: 'ft'`) — 学習的インポート

```ts
type FtBatch = {
  kind: 'ft';
  task: 'classification' | 'causal-lm';
  examples: FtExample[];
  dataset?: string;
  model?: Record<string, unknown>;
};
type FtExample =
  | { input: string; label: string }                          // classification
  | { messages: { role: string; content: string }[] }         // causal-lm (chat)
  | { prompt: string; completion: string };                   // causal-lm (prompt/completion)
```

- `FtSink` がデータセット (JSONL) + ジョブ manifest に materialize → 外部 runner を起動。
  出力レイアウトは [data/ft-dataset.md](./ft-dataset.md)。
- `task` で `examples` の形が変わる。`toRecord` が不正な形を fail-fast。

関連: [feature/save-stage.md](../feature/save-stage.md) / [feature/ft-import.md](../feature/ft-import.md)
