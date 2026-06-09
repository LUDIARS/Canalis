// 注入 LlmExecutor (local-openai 等) を、 カスケードの 1 段 (TierScorer) に橋渡しする。
// = LLM を「分類器」として使う Tier1/Tier2。 core は LLM を知らないまま、 ② が executor を注入する。

import type { LlmExecutor } from './local-openai.js';
import type { ScoreInput, ScoreResult, TierScorer } from './types.js';

export type ExecutorScorerOptions = {
  /** 注入する LLM executor (例: localOpenAiExecutor(...))。 */
  executor: LlmExecutor;
  /** 候補ラベル既定 (input.labels が優先)。 */
  labels?: string[];
  /** system 指示の上書き。 */
  system?: string;
  /** executor へ渡すモデル上書き。 */
  model?: string;
  /** ラベルが取れたときの confidence (LLM は確率を返さないため固定。 既定 0.85)。 */
  confidence?: number;
};

const DEFAULT_SYSTEM =
  'You are a strict text classifier. Reply with EXACTLY ONE label from the allowed list and nothing else.';

/** LlmExecutor を分類用 TierScorer にする。 候補外応答は abstain (null) → 次段へ。 */
export function executorScorer(opts: ExecutorScorerOptions): TierScorer {
  const fixed = opts.confidence ?? 0.85;
  return async (input: ScoreInput): Promise<ScoreResult | null> => {
    const labels = input.labels ?? opts.labels ?? [];
    if (labels.length === 0) {
      throw new Error('executorScorer: labels required (opts.labels or input.labels)');
    }
    const res = await opts.executor({
      system: opts.system ?? DEFAULT_SYSTEM,
      model: opts.model,
      prompt: `Allowed labels: ${labels.join(', ')}\n\nText:\n${input.text}\n\nLabel:`,
    });
    const picked = matchLabel(res.text, labels);
    if (!picked) return null;
    return { label: picked, confidence: fixed, tier: 0 }; // tier は Cascade が上書き
  };
}

/** LLM 出力から候補ラベルを 1 つ拾う (完全一致優先 → 部分一致)。 */
function matchLabel(raw: string, labels: string[]): string | null {
  const lower = raw.trim().toLowerCase();
  for (const l of labels) if (lower === l.toLowerCase()) return l;
  for (const l of labels) if (lower.includes(l.toLowerCase())) return l;
  return null;
}
