# feature: ② Clean/Categorize ステージ (整形・分類)

## 目的

`RawRecord[]` をサービス内部仕様 = sink ネイティブの「最終形」エンベロープへ整形・分類・付与する。
**サービス固有**ステージであり、実体は各サービスリポに置く (共有 lib は契約のみ持つ)。

## 契約 (`Transform`)

```ts
interface Transform<O extends SinkBatch = SinkBatch> {
  readonly name: string;                       // manifest.transform と突合
  run(records: RawRecord[]): Promise<O[]>;      // 1 入力から複数 sink 向けを出してよい
}
```

1 つの②が `RdbBatch` + `KgBatch` を同時に出す例 (Tr = 企業→ポスグレ + 推薦タグ関係→graph) を許す。
出力エンベロープの種別は [data/sink-envelope.md](../data/sink-envelope.md)。

## なぜ共有 lib に置かないか

整形知識を②に集約することで、③writer は table-map / 射影 / サービス知識を持たずに済み、
DB エンジン 1 種につき 1 writer を全サービスで共有できる (DESIGN §0.2 / §3)。

## LLM 排除と opt-in 機構

- 整形は決定論 (セレクタ / 正規表現 / Lector / 辞書 / ルール) を**第一**とする。
- LLM が避けられない場合のみ②が `@ludiars/canalis/llm` の機構を opt-in で使う:
  - `Cascade` — 安い段から評価し確信が出た段で打ち切る分類カスケード
    (Tier0=決定論 `lexiconScorer` / Tier1=注入ローカルモデル / Tier2=注入 LLM)。各段 `null`=abstain で次段へ。
  - `localOpenAiExecutor` — OpenAI 互換 `/v1/chat/completions` を叩く注入型 `LlmExecutor`
    (Ollama / vLLM / LM Studio / llama.cpp、例 ローカル Gemma)。
  - `toClassificationExample` — カスケード結果を FT(分類) 教師に変換 (tier フィルタで silver/gold 選別 → 自己改善ループ)。
- LLM 失敗は throw。②側で握って決定論フォールバックに倒す (LLM は従、heuristic が主)。
- core/crawl/save は `llm` を import も呼出もしない。

## 参考 (リファレンス実装の置き場)

Tr=`tr:company-normalize` / Di=`di:comment-sentiment` 等、名前は各リポの登録 (core はグローバル registry を持たない)。

関連: [interface/llm-executor.md](../interface/llm-executor.md) / [data/sink-envelope.md](../data/sink-envelope.md)
