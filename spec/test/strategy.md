# test: テスト戦略

ランナー = vitest (`npm test` = `vitest run`)。CI (`.github/workflows`) は
`npm ci → typecheck → test → build` を Node 22 / ubuntu で実行。実体は `test/`。

設計原則 (DESIGN §0.1) どおり、決定論処理が主なのでテストは外部通信せず **fake / 注入**で契約を固定する。
各 ①source は `fetch`/`api`/`fetcher`、②/③は executor/runner を注入できるよう作られている。

## 種別と担保するもの

| 種別 | 対象テスト | 何を担保するか |
|---|---|---|
| 型/ビルドチェック | `npm run typecheck` / `npm run build` | tsc が通る (契約の型整合) |
| ①→RawRecord 契約 | `notion-source` / `notion-public-source` / `crawl/youtube` / `crawl/reddit` / `crawl/website` / `discord-source` / `discord-guild-source` | fake な API/fetcher で、各 source が `RawRecord` (source/sourceId/fetchedAt/text/raw/meta) を仕様どおり詰めること |
| manifest ローダ | `core/manifest-loader` | YAML/JSON のパースと必須フィールド (pipeline/crawl) バリデーション |
| ②→③(ft) 契約 | `ft-writer` | `FtSink` が data.jsonl + job.json を materialize し fake runner を起動すること (実 FT はしない) |
| 分類カスケード | `cascade` | Tier0(決定論) で取れれば LLM を呼ばず、取れなければ次段へ委ね tier を記録すること |
| LLM executor | `local-openai` | fetch 差し替えで chat/completions のリクエスト整形・応答解釈・usage マップ・エラー処理 |
| config ストア | `config-store` | token 暗号化往復で復元、非シークレット平文、解決順 (env 優先→config)、マスク表示 |
| ③実 DB 結合 | `save/kuzu-integration` / `save/postgres-integration` | 生成 SQL/Cypher が実 DB で通電すること |

## 実 DB 結合テストの前提

- **kuzu**: in-process embedded (外部サーバ不要)。`kuzu` npm を devDependency で使い、tmpdir に DB 作成 → 各テスト後に削除。
- **postgres**: `TEST_POSTGRES_URL` 未設定なら全テスト skip (CI 既定は省略)。
  例: `TEST_POSTGRES_URL=postgres://user:pass@localhost:5432/testdb npm test -- postgres-integration`。

## 担保していない範囲 (現状)

- 実 Notion / YouTube / Reddit / Discord API への live 通信 (fake で契約固定のみ)。
- notion-public の実ブラウザレンダリング (`PlaywrightFetcher`) は注入差し替えで回避。
- FT runner (Python) の実学習 (runner は fake。`runners/ft/` の `--dry-run` でデータ契約のみ確認可)。

関連: [feature/pipeline-overview.md](../feature/pipeline-overview.md) / [interface/sink-executors.md](../interface/sink-executors.md)
