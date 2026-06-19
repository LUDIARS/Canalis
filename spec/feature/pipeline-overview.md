# feature: 3 ステージパイプライン (Canalis 全体)

## 目的

外部データを **① crawl/scrape → ② clean/categorize → ③ save/import** の 3 ステージで取り込む
LUDIARS 共有データ取込パイプライン基盤 (略称 Ci)。各ステージは型 (契約) で疎結合に接続し、
サービスは **マニフェスト**で流れを宣言する。Tirocinium の Notion クローラ / Discutere の外部コメント
収集を共通基盤に載せ替えるために作られた。

## 設計原則 (鉄則)

- **LLM をできる限り排除する。** 共有 lib `@ludiars/canalis` は LLM 依存ゼロ (SDK / API キー禁止)。
  整形は決定論 (セレクタ / 正規表現 / Lector / 辞書 / ルール) を第一とし、LLM はサービス②の中の
  opt-in に限る (別サブパス `@ludiars/canalis/llm` に隔離、core/crawl/save は import しない)。
- **整形の知識は②に集約、③は verbatim 書込。** ③writer は DB エンジン 1 種につき 1 個で全サービス共有。
- **共有 lib は 1 サービス専用ロジックを持たない。** ②Transform は各サービスリポに置く。
- **③writer に DB ドライバを直接依存させない。** `SqlExecutor` / `CypherExecutor` を注入。

## 振る舞い (入力 → 処理 → 出力)

`runPipeline(manifest, deps)` (`src/runner/runner.ts`) が manifest を解釈して実行する:

1. **① Crawl** — `manifest.crawl.sources[]` の各 source を実行し `RawRecord[]` を集約
   (replay 時は `deps.replayLoader` で raw store から読む)。
2. **raw 保存 (任意)** — `manifest.crawl.rawSave` があれば `RawRecord[]` を verbatim 保存
   (replay 時はスキップ = 元を上書きしない)。
3. **② Transform (任意)** — `manifest.transform` 名で `deps.transforms` を解決し
   `RawRecord[]` → `SinkBatch[]` へ整形。
4. **③ Save** — `manifest.save[]` の各 sink へ、`batch.kind === spec.accepts` で振り分けて書込。

出力 = `RunReport { pipeline, mode: 'crawl'|'replay', recordCount, rawSaved, written[], errors[] }`。
各ステージのエラーは throw せず `errors[]` に蓄積する (sink adapter 未知・accepts 不一致・transform 未知は throw)。

## 新サービス追加コスト

**②plugin 1 本 + manifest 1 枚。** ①は source が被れば流用、③は DB エンジンが被れば無改修。

## 制約 / 現状

- scaffold (v0.1)。①Notion adapter 移植済 / 契約・runner・③writer 骨組み済。
- ②のリファレンス実装は各サービス側 (Tr=company-normalize / Di=comment-sentiment)。
- 詳細は `DESIGN.md`。

関連: [feature/crawl-stage.md](./crawl-stage.md) / [feature/transform-stage.md](./transform-stage.md) / [feature/save-stage.md](./save-stage.md) / [feature/manifest.md](./manifest.md)
