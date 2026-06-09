// Tier0: 決定論スコアラ (辞書 + 信号)。 LLM・外部依存なし・純関数。
// 用語/emoji のラベル別重みを合算し、 最大ラベルを返す。 閾値未満は abstain (null)。
// 辞書はサービスが注入する (Canalis は機構のみ。 例: Di は Plutchik 感情辞書を渡す)。

import type { ScoreInput, ScoreResult, TierScorer } from './types.js';

export type WeightMap = Record<string, number>; // label -> weight

export type LexiconConfig = {
  /** 用語 → ラベル別重み。 例: { amazing: { joy: 1 }, boring: { boredom: 1 } } */
  terms: Record<string, WeightMap>;
  /** emoji → ラベル別重み。 本文中 emoji と signals.emojis の両方に適用。 */
  emoji?: Record<string, WeightMap>;
  /** 採用に必要な最小合計重み。 これ未満は abstain。 既定 1。 */
  minWeight?: number;
  /** 大文字小文字を無視 (既定 true)。 */
  caseInsensitive?: boolean;
};

/** text 内の term 出現回数 (単純部分一致)。 */
function countOccurrences(haystack: string, needle: string): number {
  if (!needle) return 0;
  return haystack.split(needle).length - 1;
}

function addWeights(into: Record<string, number>, weights: WeightMap, times: number): void {
  for (const [label, w] of Object.entries(weights)) {
    into[label] = (into[label] ?? 0) + w * times;
  }
}

/** LexiconConfig から Tier0 スコアラを作る。 */
export function lexiconScorer(config: LexiconConfig): TierScorer {
  const ci = config.caseInsensitive ?? true;
  const minWeight = config.minWeight ?? 1;
  // 用語辞書を ci 済キーに正規化 (毎回 lower しないよう前処理)。
  const terms = ci ? lowerKeys(config.terms) : config.terms;
  const emoji = config.emoji ?? {};

  return (input: ScoreInput): ScoreResult | null => {
    const text = ci ? input.text.toLowerCase() : input.text;
    const scores: Record<string, number> = {};
    let total = 0;

    for (const [term, weights] of Object.entries(terms)) {
      const n = countOccurrences(text, term);
      if (n > 0) { addWeights(scores, weights, n); total += sumWeights(weights) * n; }
    }
    // emoji は本文 + signals.emojis の両方。 emoji は ci 対象外。
    const emojiSources = [input.text, ...(input.signals?.emojis ?? [])].join(' ');
    for (const [glyph, weights] of Object.entries(emoji)) {
      const n = countOccurrences(emojiSources, glyph);
      if (n > 0) { addWeights(scores, weights, n); total += sumWeights(weights) * n; }
    }

    if (total < minWeight) return null; // abstain → 次段へ

    const [label, top] = argmax(scores);
    if (!label) return null;
    return { label, confidence: top / total, tier: 0, scores };
  };
}

function lowerKeys(obj: Record<string, WeightMap>): Record<string, WeightMap> {
  const out: Record<string, WeightMap> = {};
  for (const [k, v] of Object.entries(obj)) out[k.toLowerCase()] = v;
  return out;
}

function sumWeights(w: WeightMap): number {
  let s = 0;
  for (const v of Object.values(w)) s += v;
  return s;
}

function argmax(scores: Record<string, number>): [string | undefined, number] {
  let best: string | undefined;
  let bestVal = -Infinity;
  for (const [k, v] of Object.entries(scores)) {
    if (v > bestVal) { best = k; bestVal = v; }
  }
  return [best, bestVal === -Infinity ? 0 : bestVal];
}
