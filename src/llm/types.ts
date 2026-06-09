// 安価推論カスケードの契約。 ②が「LLM をできる限り使わずに分類/スコアリング」するための機構。
// Canalis core は LLM SDK を import しない: ここは「段(tier)を束ねる枠組み + 決定論 Tier0」だけ。
// Tier1 (ローカル小型モデル) / Tier2 (LLM) は consumer が注入する (TierScorer)。

/** 付帯信号 (本文以外の手がかり)。 例: upvotes / 本文外 emoji。 */
export type Signals = { upvotes?: number; emojis?: string[]; [k: string]: unknown };

/** スコアリング入力。 labels を渡すと候補ラベルを制約できる (Tier 実装が解釈)。 */
export type ScoreInput = { text: string; signals?: Signals; labels?: string[] };

/** スコアリング結果。 tier = 何段目が解決したか (0=辞書/信号, 1=ローカルモデル, 2=LLM)。 */
export type ScoreResult = {
  label: string;
  /** 0..1。 cascade の閾値判定に使う。 */
  confidence: number;
  /** どの段が解決したか (cascade が設定)。 FT の教師選別に使える。 */
  tier: number;
  /** ラベル別スコア (任意)。 */
  scores?: Record<string, number>;
};

/**
 * 1 段のスコアラ。 **null = abstain (確信なし → 次段へ委ねる)**。
 * Tier0=決定論 (lexiconScorer), Tier1=注入ローカルモデル, Tier2=注入 LLM。
 */
export type TierScorer = (input: ScoreInput) => Promise<ScoreResult | null> | ScoreResult | null;
