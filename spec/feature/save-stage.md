# feature: ③ Save/Import ステージ (保存先 writer)

## 目的

②が出した最終形エンベロープ (または①の `RawRecord[]`) を保存先へ **verbatim** 書き込む汎用ステージ。
writer (`Sink` 実装) を足せば保存先が増える。実体は `src/save/`。

## 契約 (`Sink`)

```ts
interface Sink<P = unknown> {
  readonly name: string;
  readonly accepts: SinkKind;   // 'raw' | 'rdb' | 'kg' | 'ft' — runner のルーティングに使う
  write(payload: P): Promise<void>;
}
```

runner は `manifest.save[].accepts` と sink の `accepts` 一致を要求し、`batch.kind === accepts` の
バッチだけを各 sink へ振り分ける (不一致は throw)。

## 実装済 writer (実物)

| writer | `accepts` | 受け取り | 保存先 | DB ドライバ |
|---|---|---|---|---|
| `JsonlRawSink` | `raw` | `RawRecord[]` | JSONL ファイル | なし (node fs のみ) |
| `PostgresSink` | `rdb` | `RdbBatch` | Postgres | `SqlExecutor` を注入 |
| `KuzuSink` | `kg` | `KgBatch` | Kuzu (グラフ DB) | `CypherExecutor` を注入 |
| `FtSink` | `ft` | `FtBatch` | 学習データセット + runner 起動 | runner を注入 ([feature/ft-import.md](./ft-import.md)) |

## 振る舞い

- **③は table-map / 射影 / サービス知識を持たない。** ②が作った最終形をそのまま SQL/Cypher 化して書くだけ。
- `PostgresSink`: 1 行 = 1 `INSERT ... ON CONFLICT (conflictKey) DO UPDATE SET …`。識別子は安全引用、想定外は fail-fast。
- `KuzuSink`: node = `MERGE (n:label {key:$key}) SET …`、edge = `MATCH … MERGE (a)-[r]->(b) SET …`。
  **node/edge テーブルの事前マイグレーションはサービスの責務** (Kuzu 厳格スキーマ)。
- **DB ドライバは共有 lib に直接依存させない**。consumer が postgres / kuzu driver で executor を実装して注入。

## 制約 / 現状

- postgres / kuzu の実 DB 結合テストは `test/save/` にあり (kuzu=in-process embedded、postgres=`TEST_POSTGRES_URL` 未設定時 skip)。
  本番接続コードは consumer 側 executor 実装に依存。

関連: [data/sink-envelope.md](../data/sink-envelope.md) / [data/raw-jsonl-store.md](../data/raw-jsonl-store.md) / [interface/sink-executors.md](../interface/sink-executors.md)
