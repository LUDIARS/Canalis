# interface: opt-in ローカル LLM executor (`@ludiars/canalis/llm`)

②Transform が opt-in で注入する LLM 機構の境界。**別サブパスに隔離**され、core/crawl/save は import しない
(DESIGN §0.1)。SDK 依存・API キー要求は無い (raw fetch + baseUrl のみ)。正本: `src/llm/`。

## LlmExecutor (注入 contract)

```ts
type LlmRequest = { prompt: string; system?: string; model?: string; maxTokens?: number; timeoutMs? };
type LlmResponse = { text: string; usage?: { inputTokens?: number; outputTokens?: number } };
type LlmExecutor = (req: LlmRequest) => Promise<LlmResponse>;
```

`SqlExecutor` / `CypherExecutor` / `FtRunner` と同じ「driver は注入」思想。失敗は throw (②側で握って heuristic にフォールバック)。

## localOpenAiExecutor

OpenAI 互換 `/v1/chat/completions` を叩く `LlmExecutor` を作る。

```ts
localOpenAiExecutor({ baseUrl, model, apiKey?, timeoutMs?, fetchImpl? }): LlmExecutor;
```

- 対応: Ollama / vLLM / LM Studio / llama.cpp server (例 Ollama `http://localhost:11434/v1`, model `gemma…`)。
- POST `<baseUrl>/chat/completions` に `{ model, max_tokens, messages, stream:false }`。
  `system` があれば system message を前置。`apiKey` 設定時のみ `Authorization: Bearer`。
- 既定 `max_tokens=4096` / `timeout=120000ms` (reasoning モデルの空応答を防ぐため広め)。
- 非 200 / 空 `message.content` は throw。`usage` (prompt/completion tokens) をマップ。

## 分類カスケード機構 (LLM を極力呼ばない枠)

- `Cascade(tiers, { minConfidence? })` — Tier0=決定論 `lexiconScorer` / Tier1=注入ローカルモデル / Tier2=注入 LLM の順で評価。
  各段 `TierScorer` が `null`=abstain なら次段、確信が出た段で打ち切り `ScoreResult.tier` に段 index を記録。
- `lexiconScorer(config)` — 辞書・信号ベースの決定論スコアラ (LLM 非依存)。
- `executorScorer(opts)` — 注入 `LlmExecutor` を `TierScorer` 化。
- `toClassificationExample(text, result)` — カスケード結果を FT(分類) 教師に変換 (tier フィルタで silver/gold 選別)。

関連: [feature/transform-stage.md](../feature/transform-stage.md) / [feature/ft-import.md](../feature/ft-import.md)
