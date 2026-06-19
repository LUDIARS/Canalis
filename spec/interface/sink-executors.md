# interface: ③Sink の DB ドライバ注入 contract

③writer は DB ドライバに直接依存しない。consumer (サービス側) が driver で executor 関数を実装して注入する
(`SqlExecutor` / `CypherExecutor`)。これにより共有 lib は postgres / kuzu パッケージを依存に持たない。

## SqlExecutor (PostgresSink)

```ts
type SqlExecutor = (query: string, params: unknown[]) => Promise<void>;
new PostgresSink(exec: SqlExecutor, name = 'postgres');
```

- パラメタ化クエリ (`$1, $2, …`) を実行する関数。値は返さない (`Promise<void>`)。
- 例 (postgres.js): `(q, p) => sql.unsafe(q, p as any).then(() => {})`。
- `PostgresSink` が生成する SQL は `INSERT INTO "table" (…) VALUES ($1,…) ON CONFLICT (…) DO UPDATE SET …`。

## CypherExecutor (KuzuSink)

```ts
type CypherExecutor = (query: string, params: Record<string, unknown>) => Promise<void>;
new KuzuSink(exec: CypherExecutor, config?: { keyProp?: string }, name = 'kuzu');
```

- named パラメタ (`$key`, `$p_<prop>`, `$fromKey`, `$toKey`) の dict を渡す。
- 例: `(q, p) => conn.query(q, p).then(() => {})`。
- `KuzuSink` が生成する Cypher は `MERGE (n:Label {key:$key}) SET …` / `MATCH … MERGE (a)-[r:Rel]->(b) SET …`。
- **前提**: 対象 node/edge テーブルは事前にマイグレーションで作成済 (Kuzu 厳格スキーマ、サービスの責務)。

## raw / ft sink

- `JsonlRawSink` は driver 不要 (node fs のみ) → [data/raw-jsonl-store.md](../data/raw-jsonl-store.md)。
- `FtSink` は `FtRunner` を注入 (外部プロセス起動) → [interface/ft-runner.md](./ft-runner.md)。

関連: [feature/save-stage.md](../feature/save-stage.md) / [data/sink-envelope.md](../data/sink-envelope.md)
